/*=============================================================================
	UnNavigationMesh.cpp:

  UNavigationMesh and subclass functions

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "UnPath.h"
#include "UnTerrain.h"
#include "EngineAIClasses.h"
#include "EngineMaterialClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "DebugRenderSceneProxy.h"
#include "GenericOctree.h"

#if WITH_RECAST
#include "RecastNavMesh.h"
#endif

IMPLEMENT_CLASS(UNavigationMeshBase);
IMPLEMENT_CLASS(APathTargetPoint);
IMPLEMENT_CLASS(APylonSeed);
IMPLEMENT_CLASS(UInterface_PylonGeometryProvider);

// >>>>>>>>>>> Global (static) Navmesh generation parameters 
// Number of directions to expand in. (note this should be 8..  if it's not 8 the mesh will be misaligned)
#define NAVMESHGEN_NUM_ARC_TESTS 8

// maximum number of times to subdivide stepsize when an obstacle is encountered 
INT	  ExpansionMaxSubdivisions = 2;
#define NAVMESHGEN_MAX_SUBDIVISIONS ExpansionMaxSubdivisions

// epsilon used in determining if verts form a straight line
FLOAT ExpansionStraightLineEpsilon = 10.f;
#define NAVMESHGEN_STRAIGHTLN_EPSILON ExpansionStraightLineEpsilon

// epsilon used for detemrining if verts form a straight line, in 3D (as opposed to 2-d only math which used the above var)
FLOAT ExpansionStraightLineEpsilon3D = 1.0f;
#define NAVMESHGEN_STRAIGHTLN_EPSILON3D ExpansionStraightLineEpsilon3D

// epsilon for testing convexity
FLOAT ExpansionConvexTolerance = 0.1f;
#define NAVMESHGEN_CONVEX_TOLERANCE ExpansionConvexTolerance

// epsilon for testing convexity of edges during edge smoothing (usually should match NAVMESHGEN_CONVEX_TOLERANCE)
FLOAT ExpansionEdgeSmoothingConvexTolerance = 30.f;
#define NAVMESHGEN_EDGESMOOTHING_CONVEX_TOLERANCE ExpansionEdgeSmoothingConvexTolerance

// threshold used to determine if a subdivision should be pursued or not (e.g. if we've already done one closer than this thresh, don't subdivide)
FLOAT ExpansionSubdivisionDistPctThresh = 1.05f;
#define	NAVMESHGEN_SUBDIVISION_DISTPCT_THRESH ExpansionSubdivisionDistPctThresh

// any gaps along poly edges above this threshold will get obstacle geo added for it
FLOAT ExpansionObstacleMeshGapEpsilon = 1.5f;
#define NAVMESHGEN_OBSTACLEMESH_GAP_EPSILON ExpansionObstacleMeshGapEpsilon

// the amount under the poly that the bounds of the poly will extend as a buffer
FLOAT ExpansionPolyBoundsDownOffset = 15.0f;
#define NAVMESHGEN_POLYBOUNDS_DOWN_OFFSET ExpansionPolyBoundsDownOffset

// the maximum value allowed for dotproduct of dwo adjacent edges in a poly before the poly is considered invalid
FLOAT ExpansionMaxPolyEdgeDot = 1.0f; 
#define NAVMESHGEN_MAX_POLY_ANGLE_DOT ExpansionMaxPolyEdgeDot

// the maximum size (max number of verts) of a concave slab during slab merging
INT ExpansionMaxConcaveSlabVertCount = 500;
#define NAVMESHGEN_MAX_SLAB_VERT_COUNT ExpansionMaxConcaveSlabVertCount

// used when determining merge candidates.  If a new merged poly is > this distance from all control points (verts) of polys used ot merge the new poly
// the merge will not be allowed
FLOAT ExpansionMaxSquareMergeControlPtThresh = 15.0f;
#define NAVMESHGEN_POLY_MERGE_DIST_THRESH ExpansionMaxSquareMergeControlPtThresh

// used for lowering bounds of Recast generated poly (don't modify, set by NavMeshPass_Recast)
FLOAT RecastBoundsZOffset = 80.0f;

// used for snapping nearby vertices to other vertices (don't modify, set by NavMeshPass_Recast)
FVector RecastSnapExtent(50.0f, 50.0f, 50.0f);

// used for snapping nearby vertices to edges (don't modify, set by NavMeshPass_Recast)
FVector RecastEdgeSnapExtent(80.0f, 80.0f, 80.0f);

// DEBUG stage control vars (to disable/enable portions of the generation process selectively)
// do any simplification at all?
UBOOL ExpansionDoSimplification = TRUE;
// do 3->2 merging?
UBOOL ExpansionDoThreeToTwoMerge = FALSE;
// do basic adjacent convex poly merging?
UBOOL ExpansionDoPolyMerge = TRUE;
// do concave poly slab merging?
UBOOL ExpansionDoPolyConcaveMerge = TRUE;
// do A* square expansion merge?
UBOOL ExpansionDoSquareMerge = TRUE;
// do final save structure fixup?
UBOOL ExpansionDoSaveFixup = TRUE;
// cull small polys?
UBOOL ExpansionCullPolys = TRUE;
// build the obstacle mesh?
UBOOL ExpansionBuildObstacleMesh = TRUE;
// create any edge connections between polys?
UBOOL ExpansionCreateEdgeConnections = TRUE;
// merge polys in-place created from subdivided iterations?
UBOOL ExpansionDoSubdivisionMerging = TRUE;
// smooth boundary edges?
UBOOL ExpansionDoEdgeSmoothing = TRUE;
// when this is enabled no simplification, or subdivision merging will take place.  Useful for debugging exploration problems
UBOOL ExpansionDoRawGridOnly = FALSE;
// when enabled only concave slabs will be merged and nothing else will take place.. allows easy debug of slab generation
UBOOL ExpansionDoConcaveSlabsOnly = FALSE;
// when used when ExpansionDoConcaveSlabsOnly  is ON, but you still want edge simplification
UBOOL ExpansionDoEdgeSimplificationEvenInConcaveSlabMode = TRUE;
// draw polys used to create dropdown edges?
UBOOL ExpansionDrawDropDownPolys = FALSE;
// draw a line from each poly to its expansion parent?
UBOOL ExpansionDrawPolyParents = FALSE;
// by default subdivided polys are snapped to their parent's height.. this allows you to disable that (very useful for debugging weird subdivision polys)
UBOOL ExpansionDisableSubdivisionHeightSnapping = FALSE;
// disable vert maxheight slope calculations (see AddSquarePolyFromExtent)?
UBOOL ExpansionDisableVertMaxHeightSlopeMax = FALSE;
// disable adjacent poly vert alignemtn 
UBOOL ExpansionDoAdjacentPolyVertAlignment = TRUE;

#define DO_SIMPLIFICATION (ExpansionDoSimplification && !ExpansionDoRawGridOnly)
#define DO_OBSTACLEMESH_CREATION (ExpansionBuildObstacleMesh && !ExpansionDoRawGridOnly && !ExpansionDoConcaveSlabsOnly )
#define DO_EDGE_CREATION (ExpansionCreateEdgeConnections && !ExpansionDoRawGridOnly && !ExpansionDoConcaveSlabsOnly )
#define DO_SUBDIV_MERGE (ExpansionDoSubdivisionMerging && !ExpansionDoRawGridOnly)
#define NAVMESHGEN_DO_EDGESMOOTHING ExpansionDoEdgeSmoothing
#define EXPANSION_TRACE_FLAGS TRACE_Others|TRACE_Volumes|TRACE_World|TRACE_Blocking|TRACE_Movers
#define NAVMESHGEN_DRAW_DROPDOWN_POLYS ExpansionDrawDropDownPolys
#define NAVMESHGEN_DRAW_POLY_PARENTS ExpansionDrawPolyParents
#define NAVMESHGEN_DISABLE_SUBDIVISION_HEIGHT_SNAPPING ExpansionDisableSubdivisionHeightSnapping
#define NAVMESHGEN_DISABLE_VERT_MAXHEIGHT_SLOPE_MAX ExpansionDisableVertMaxHeightSlopeMax
// misc debug only vars
UBOOL ExpansionIntersectDebug = FALSE;
// DEBUG only - will run lots of linechecks and draw output to test obstacle mesh
UBOOL ExpansionTestCollision = FALSE;


// >>>>>>>>>>> NavMesh generation parameters taken from the scout
// Size of our expansion step. (also the size of the base square added at each step to the mesh)
#define NAVMESHGEN_STEP_SIZE AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_StepSize

// half height of expansion tests done (this should be the half height of your smallest pathing entity)
#define NAVMESHGEN_ENTITY_HALF_HEIGHT AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_EntityHalfHeight

// starting offset for ground checks done during each expansion step
#define NAVMESHGEN_STARTING_HEIGHT_OFFSET AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_StartingHeightOffset

// maximum valid height for ledges to drop before no expansion is allowed
#define NAVMESHGEN_MAX_DROP_HEIGHT AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_MaxDropHeight

// maximum height to consider valid for step-ups 
#define NAVMESHGEN_MAX_STEP_HEIGHT AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_MaxStepHeight

// thresh above which vert snapping is performed (see comment in Scout.uc)
#define NAVMESHGEN_VERT_ZDELTASNAP_THRESH AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_VertZDeltaSnapThresh

// minimum area of polygons (below threshold will be culled)
#define NAVMESHGEN_MIN_POLY_AREA AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_MinPolyArea

// multiplier of NAVMESHGEN_STEP_SIZE used to determine if small area mindot or large area mindot should be used
#define NAVMESHGEN_MERGE_DOT_AREA_THRESH AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_MinMergeDotAreaThreshold

// minimum dot product necessary for merging polys of an area below NAVMESHGEN_MERGE_DOT_AREA_THRESH
#define NAVMESHGEN_MERGE_DOT_SMALL_AREA AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_MinMergeDotSmallArea

// minimum dot product necessary for merging polys of an area above NAVMESHGEN_MERGE_DOT_AREA_THRESH
#define NAVMESHGEN_MERGE_DOT_LARGE_AREA AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_MinMergeDotLargeArea

// minimum dot product necessary for merging during concave poly merge
FLOAT ExpansionMinConcaveMergeDot = 0.996f;
#define NAVMESHGEN_MERGE_DOT_CONCAVEMERGE ExpansionMinConcaveMergeDot

// maximum height to check height against (should be the height of your biggest entity)
#define NAVMESHGEN_MAX_POLY_HEIGHT					AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_MaxPolyHeight
#define NAVMESHGEN_MAX_POLY_HEIGHT_PYLON(THEPYLON) ((THEPYLON != NULL && THEPYLON->MaxPolyHeight_Optional>0.f) ? THEPYLON->MaxPolyHeight_Optional : NAVMESHGEN_MAX_POLY_HEIGHT)

// height threshold used when determining if two polys can be merged (e.g. if the two poly heights are within this value, they are OK to merge)
#define NAVMESHGEN_POLY_HEIGHT_MERGE_THRESH AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_HeightMergeThreshold

// maximum distance between paralel edges to be considered valid connection candidates
#define NAVMESHGEN_MAX_EDGE_DELTA AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_EdgeMaxDelta

// minimum Z value in normal of surfaces which are walkable
#define NAVMESHGEN_MIN_WALKABLE_Z AScout::GetGameSpecificDefaultScoutObject()->WalkableFloorZ

// maximum extent size to be used for ground checks
#define NAVMESHGEN_MAX_GROUNDCHECK_SIZE AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_MaxGroundCheckSize

#define NAVMESHGEN_MIN_EDGE_LENGTH AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_MinEdgeLength

#define NAVMESHGEN_EXPANSION_DO_OBSTACLE_MESH_SIMPLIFICATION AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_ExpansionDoObstacleMeshSimplification


#define NAVMESHGEN_GRIDSNAPSIZE (NAVMESHGEN_STEP_SIZE * 2.0f)



#define PERF_NAVMESH_TIMES 0

// LOOKING_FOR_PERF_ISSUES
#if PERF_NAVMESH_TIMES
TMap<FName, FLOAT> OverallTaskTracking;
void TrackTaskTime(FName TaskName, FLOAT NewTaskIncrement)
{
	FLOAT* Current_Val = OverallTaskTracking.Find(TaskName);
	FLOAT NewVal=NewTaskIncrement;
	if(Current_Val != NULL)
	{
		NewVal += *Current_Val;
	}

	OverallTaskTracking.Set(TaskName,NewVal);
}

IMPLEMENT_COMPARE_CONSTREF(FLOAT, LargestTimeFirst, { return appCeil(A-B); })

void PrintTaskStats()
{
	FLOAT TotalTime=0.f;
	
	for(TMap<FName, FLOAT>::TIterator It(OverallTaskTracking);It;++It)
	{
		TotalTime+=It.Value();
	}

	OverallTaskTracking.ValueSort<COMPARE_CONSTREF_CLASS(FLOAT,LargestTimeFirst)>();
	for(TMap<FName, FLOAT>::TIterator It(OverallTaskTracking);It;++It)
	{
		FLOAT Time=It.Value();
		debugf(TEXT("TOTAL TIME FOR TASK %s %.2f(ms)) -- %d mins %d secs (%.2f%% of total)"),*It.Key().ToString(),Time*1000.f,appFloor(Time/60.f), appFloor(Time)%60,(Time/TotalTime)*100.f);
	}
	debugf(TEXT("Total Time: %dm%ds"),appFloor(TotalTime/60.f), appFloor(TotalTime)%60);
}
void ResetTaskStats()
{
	OverallTaskTracking.Empty();
}

#define SCOPE_QUICK_TIMERX(TIMERNAME,TRACKTOTAL) \
	static DOUBLE AverageTime##TIMERNAME = -1.0; \
	static INT NumItts##TIMERNAME = 0; \
	static DOUBLE TotalTime##TIMERNAME = 0.f;\
class TimerClassName##TIMERNAME \
{ \
public:\
	TimerClassName##TIMERNAME() \
{ \
	StartTimerTime = appSeconds(); \
} \
	\
	~TimerClassName##TIMERNAME() \
{\
	FLOAT Duration = appSeconds() - StartTimerTime;\
	if(TRACKTOTAL) TrackTaskTime(TEXT(#TIMERNAME),Duration);\
	if(AverageTime##TIMERNAME < 0.f)\
{\
	AverageTime##TIMERNAME = Duration;\
}\
			else\
{\
	AverageTime##TIMERNAME += (Duration - AverageTime##TIMERNAME)/++NumItts##TIMERNAME;\
}\
	\
	debugf(TEXT("Task %s took %.2f(ms) Avg %.2f(ms) -- %d mins %d secs"),TEXT(#TIMERNAME),Duration*1000.f,AverageTime##TIMERNAME*1000.f, appFloor(Duration/60.f), appFloor(Duration)%60);\
}\
	DOUBLE StartTimerTime;\
};\
	TimerClassName##TIMERNAME TIMERNAME = TimerClassName##TIMERNAME();
#else
#define SCOPE_QUICK_TIMERX(blah,TRACKTOTAL) {}
#define TrackTaskTime(TaskName){}
#define PrintTaskStats(){}
#define ResetTaskStats(){}
#endif

static INT TotalNumOfPylons;
static INT CurrentPylonIndex;

#if ENABLE_VECTORINTRINSICS
static const VectorRegister VBigNumber = { BIG_NUMBER,BIG_NUMBER,BIG_NUMBER,BIG_NUMBER };
static const VectorRegister VNegBigNumber = { -BIG_NUMBER,-BIG_NUMBER,-BIG_NUMBER,-BIG_NUMBER };

/**
 * Adds a list of triangles to this volume. Specialized version that has the loop unrolled and vectorized
 *
 * @param Start the starting point in the build triangles array
 * @param NumTris the number of tris to iterate through adding from the array
 * @param BuildTriangles the list of triangles used for building the collision data
 */
template<>
void NavMeshKDOP::AddTriangles(WORD StartIndex,WORD NumTris,TArray<FkDOPBuildCollisionTriangle<WORD> >& BuildTriangles)
{
	// Load the 3 planes that are in all Xs, Ys, Zs form
	VectorRegister PlanesX = VectorLoadFloat3_W0(&FkDOPPlanes::PlaneNormals[0]);
	VectorRegister PlanesY = VectorLoadFloat3_W0(&FkDOPPlanes::PlaneNormals[1]);
	VectorRegister PlanesZ = VectorLoadFloat3_W0(&FkDOPPlanes::PlaneNormals[2]);

	// Use the constant big/small numbers to push our planes into default states
	VectorRegister VMin = VBigNumber;
	VectorRegister VMax = VNegBigNumber;

	// Since we are moving straight through get a pointer to the data
	const FkDOPBuildCollisionTriangle<WORD>* RESTRICT CollisionTrianglePtr = (FkDOPBuildCollisionTriangle<WORD>*)&BuildTriangles(StartIndex);

	// Go through the list and add each of the triangle verts to our volume
	for (WORD Triangle = 0; Triangle < NumTris; Triangle++)
	{
		// Splat V0 components across the vectors
		VectorRegister PointX = VectorLoadFloat3_W0(&CollisionTrianglePtr->V0);
		VectorRegister PointY = VectorReplicate(PointX,1);
		VectorRegister PointZ = VectorReplicate(PointX,2);
		PointX = VectorReplicate(PointX,0);
		// Calculate the dot products
		VectorRegister Dot = VectorMultiplyAdd(PointZ,PlanesZ,VectorMultiplyAdd(PointY,PlanesY,VectorMultiply(PointX,PlanesX)));
		// Do the Mins/Maxes of each components
		VMin = VectorMin(Dot,VMin);
		VMax = VectorMax(Dot,VMax);

		// Splat V1 components across the vectors
		PointX = VectorLoadFloat3_W0(&CollisionTrianglePtr->V1);
		PointY = VectorReplicate(PointX,1);
		PointZ = VectorReplicate(PointX,2);
		PointX = VectorReplicate(PointX,0);
		// Calculate the dot products
		Dot = VectorMultiplyAdd(PointZ,PlanesZ,VectorMultiplyAdd(PointY,PlanesY,VectorMultiply(PointX,PlanesX)));
		// Do the Mins/Maxes of each components
		VMin = VectorMin(Dot,VMin);
		VMax = VectorMax(Dot,VMax);

		// Splat V2 components across the vectors
		PointX = VectorLoadFloat3_W0(&CollisionTrianglePtr->V2);
		PointY = VectorReplicate(PointX,1);
		PointZ = VectorReplicate(PointX,2);
		PointX = VectorReplicate(PointX,0);
		// Calculate the dot products
		Dot = VectorMultiplyAdd(PointZ,PlanesZ,VectorMultiplyAdd(PointY,PlanesY,VectorMultiply(PointX,PlanesX)));
		// Do the Mins/Maxes of each components
		VMin = VectorMin(Dot,VMin);
		VMax = VectorMax(Dot,VMax);

		CollisionTrianglePtr++;
	}

	// Store the results as our volume
	VectorStoreFloat3(VMin,Min);
	VectorStoreFloat3(VMax,Max);
}
#endif

/**
 * BuildPolyFromExtentAndHitInfo
 *  will construct a polygon from the passed location, width, and normal
 * @param Location - location (center) of poly
 * @param HitNormal - normal of the ground at this spot
 * @param HalfWidth - halfwidth of square to create
 * @param out_Poly - array containing the poly constructed
 */
void UNavigationMeshBase::BuildPolyFromExtentAndHitInfo(const FVector& Location, const FVector& HitNormal, FLOAT HalfWidth, TArray<FVector>& out_Poly)
{
	// CW order

	// top right
	FVector XY = FVector(HalfWidth,HalfWidth,0.f);
	FLOAT XYDot = Abs<FLOAT>(XY.SafeNormal() | HitNormal);
	FLOAT VertAdd = HalfWidth + (HalfWidth * XYDot);
	out_Poly.AddItem(Location + FVector(HalfWidth,HalfWidth,   -(HitNormal.X * VertAdd + HitNormal.Y *  VertAdd)));
	// bottom right
	XY = FVector(-HalfWidth,HalfWidth,0.f);
	XYDot = Abs<FLOAT>(XY.SafeNormal() | HitNormal);
	VertAdd = HalfWidth + (HalfWidth * XYDot);
	out_Poly.AddItem(Location + FVector(-HalfWidth,HalfWidth,  -(HitNormal.X * -VertAdd + HitNormal.Y *  VertAdd)));
	// bottom left
	XY = FVector(-HalfWidth,-HalfWidth,0.f);
	XYDot = Abs<FLOAT>(XY.SafeNormal() | HitNormal);
	VertAdd = HalfWidth + (HalfWidth * XYDot);
	out_Poly.AddItem(Location + FVector(-HalfWidth,-HalfWidth, -(HitNormal.X * -VertAdd + HitNormal.Y * -VertAdd)));
	// top left
	XY = FVector(HalfWidth,-HalfWidth,0.f);
	XYDot = Abs<FLOAT>(XY.SafeNormal() | HitNormal);
	VertAdd = HalfWidth + (HalfWidth * XYDot);
	out_Poly.AddItem(Location + FVector(HalfWidth,-HalfWidth,  -(HitNormal.X *  VertAdd + HitNormal.Y * -VertAdd)));
}

/**
 * MaxStepForSlope
 * - will calculate the maximum height delta along the max acceptable slope for the given lateral distance
 * @param LatDist - lateral distance to calculate max acceptable slope for
 * @return - maximum acceptable height delta for the given lateral distance
 */
FLOAT MaxStepForSlope(FLOAT LatDist)
{
	if( appIsNearlyZero(NAVMESHGEN_MIN_WALKABLE_Z))
	{
		return LatDist;
	}
	return LatDist*appTan(appAcos(NAVMESHGEN_MIN_WALKABLE_Z));
}

/**
 * AddSquarePolyFromExtent
 *  Adds a square polygon to the mesh
 * @param Poly - list of vert locations that represent the incoming polygon
 * @param Location - center of the poly we're adding
 * @param PolyHeight - the height supported at this polygon ( if -1.0f a new height will be calculated )
 * @return - the poly added 
 */
FNavMeshPolyBase* UNavigationMeshBase::AddSquarePolyFromExtent(TArray<FVector>& Poly, const FVector& Location, FLOAT PolyHeight/*=-1.0f*/) 
{
	// figure out what our max acceptable vert snap height is for this size poly 
	FLOAT SideLen = (Poly(0) - Poly(1)).Size();
	FLOAT MaxSnapHeight = MaxStepForSlope(SideLen);
	
	FNavMeshPolyBase* NewPoly = NULL;

	if(NAVMESHGEN_DISABLE_VERT_MAXHEIGHT_SLOPE_MAX)
	{
		NewPoly = AddPoly(Poly,PolyHeight,WORLD_SPACE,NAVMESHGEN_MAX_STEP_HEIGHT);
	}
	else
	{
		NewPoly = AddPoly(Poly,PolyHeight,WORLD_SPACE,Min<FLOAT>(MaxSnapHeight,NAVMESHGEN_MAX_STEP_HEIGHT));
	}
	
	if(NewPoly != NULL)
	{
		NewPoly->PolyBuildLoc = Location;
	}
	return NewPoly;
}

/**
 * AddTriangesForSlopedPoly
 *  Adds two triangles for the square passed, split perp to the slope 
 * @param Poly - list of vert locations that represent the incoming polygon
 * @param Location - center of the poly we're adding
 * @param PolyNorm - normal of poly being added
 * @return - one of the polys added 
 */
FNavMeshPolyBase* UNavigationMeshBase::AddTriangesForSlopedPoly(TArray<FVector>& Poly, const FVector& Location, const FVector& PolyNorm)
{

	// figure out what our max acceptable vert snap height is for this size poly 
	const FLOAT SideLen = (Poly(0) - Poly(1)).Size();
	FLOAT MaxSnapHeight = Min<FLOAT>(MaxStepForSlope(SideLen),NAVMESHGEN_MAX_STEP_HEIGHT);
	
	if(NAVMESHGEN_DISABLE_VERT_MAXHEIGHT_SLOPE_MAX)
	{
		MaxSnapHeight = NAVMESHGEN_MAX_STEP_HEIGHT;
	}


	

	FNavMeshPolyBase* NewPoly = NULL;
	for(INT Idx=0;Idx<4;++Idx)
	{
		const FVector Ctr = Poly(Idx) + (Poly((Idx+2)%4) - Poly(Idx))*0.5f;	
		TArray<FVector> Tri;
		Tri.AddItem(Poly(Idx));
		Tri.AddItem(Poly((Idx+1)%4));
		Tri.AddItem(Ctr);

		NewPoly = AddPoly(Tri,-1.f,WORLD_SPACE,MaxSnapHeight);
		if(NewPoly != NULL)
		{
			NewPoly->PolyBuildLoc = Location;
		}
	}

	
	return NewPoly;
}

/**
 * GetStepSize
 * - calculate the step size (step granularity) for a given subdivision iteration
 * @param SubdivisionIteration - number of subdvisions to calculate for
 * @return - calcuated step size
 */
FLOAT GetStepSize(INT SubdivisionIteration)
{
	return NAVMESHGEN_STEP_SIZE / appPow(2,SubdivisionIteration);
}

/**
 * will add a square poly to the mesh at the passed location unless it's already in the mesh
 * @param NewLocation - location of new node
 * @param HitNormal - normal of the hit which we're adding a node for
 * @param out_bInvalidSpot - OPTIONAL out param indicating whether we couldn't add a node here due to it being out of bounds
 * @param SubdivisionIteration - OPTIONAL param indicating what subdivision iteration we're adding for
 * @return - the new poly we just added (NULL if not succesful)
 */
FNavMeshPolyBase* APylon::AddNewNode(const FVector& NewLocation, const FVector& HitNormal, UBOOL* out_bInvalidSpot, INT SubdivisionIteration)
{
	APylon* ContainingPylon=NULL;
	FNavMeshPolyBase* ContainingPoly=NULL;

	static TArray<FVector> Poly;
	Poly.Reset();

	NavMeshPtr->BuildPolyFromExtentAndHitInfo(NewLocation,HitNormal,GetStepSize(SubdivisionIteration),Poly);
	UNavigationHandle::PolyIntersectsMesh(Poly,ContainingPylon,ContainingPoly,NULL,TRUE, &ImposterPylons);

	UBOOL bNewPolyAlreadyInMesh = (ContainingPoly != NULL);
	UBOOL bOutOfRange = !IsPtWithinExpansionBounds(NewLocation);

	if( bNewPolyAlreadyInMesh || bOutOfRange )
	{
		if(bOutOfRange ||  Abs<FLOAT>(ContainingPoly->PolyBuildLoc.Z - NewLocation.Z) < NAVMESHGEN_MAX_STEP_HEIGHT )
		{
			if(out_bInvalidSpot!=NULL)
			{
				*out_bInvalidSpot=TRUE;
			}
		}
		return NULL;
	}

	// if we're on a steep enough slope that we couldn't get merged, split poly into tris perp to slope direction
// 	if( SubdivisionIteration == 0 && (HitNormal | FVector(0.f,0.f,1.f)) < NAVMESHGEN_MERGE_DOT_CONCAVEMERGE )
// 	{
// 		return NavMeshPtr->AddTriangesForSlopedPoly(Poly,NewLocation,HitNormal);
// 	}
	return NavMeshPtr->AddSquarePolyFromExtent(Poly,NewLocation);
}

/**
 * overidden so that imported pylons don't get moved around by the 'based' code of navigation points	
 */
UBOOL APylon::ShouldBeBased()
{
	if(bImportedMesh)
	{
		return FALSE;
	}
	return Super::ShouldBeBased();
}

/**
 * keeps track of edges which we could drop down but not climb up for later addition to the mesh
 * 
 * @param NewLocation - destination location of dropdown 
 * @param OldLocation - source location of dropdown
 * @param HitNormal   - normal of the ground hit causing this dropdown
 * @param ParentPoly  - parent poly we're expending from 
 * @param SubdivisionIteration - current subdivision iteration 
 */
void APylon::SavePossibleDropDownEdge(const FVector& NewLocation, const FVector& OldLocation, const FVector& HitNormal, FNavMeshPolyBase* ParentPoly, UBOOL bSkipPullBack)
{
	if(NavMeshPtr->DropEdgeMesh == NULL)
	{
		// create a new one! (outer to the navmesh so there is an easy ref back to it)
		NavMeshPtr->DropEdgeMesh = Cast<UNavigationMeshBase>(StaticConstructObject(UNavigationMeshBase::StaticClass(),NavMeshPtr));
	}

	// push back toward the start location a bit so they're sure to overlap
	FVector Adjusted_NewLocation = NewLocation;
	
	if (!bSkipPullBack)
	{
		Adjusted_NewLocation += (OldLocation - NewLocation).SafeNormal() * GRIDSIZE;
	}

	Adjusted_NewLocation.Z = Max<FLOAT>(OldLocation.Z,Adjusted_NewLocation.Z);

	APylon* ContainingPylon=NULL;
	FNavMeshPolyBase* ContainingPoly=NULL;

	TArray<FVector> Poly;
	NavMeshPtr->BuildPolyFromExtentAndHitInfo(Adjusted_NewLocation,HitNormal,GetStepSize(0),Poly);
	NavMeshPtr->DropEdgeMesh->IntersectsPoly(Poly,ContainingPoly,NULL,WORLD_SPACE);
	

	if(ContainingPoly == NULL)
	{
		NavMeshPtr->DropEdgeMesh->AddPoly(Poly,NAVMESHGEN_ENTITY_HALF_HEIGHT+1.f,WORLD_SPACE);	
	}
}

/**
 * NavMeshGen_IsValidGroundHit
 * allows the scout to determien if the passed ground hit is a valid spot for navmesh to exist
 * @param Hit - the hit to determine validity for
 * @return - TRUE If the passed spot was valid
 */
UBOOL AScout::NavMeshGen_IsValidGroundHit( FCheckResult& Hit )
{
	if(Hit.Actor != NULL && !Hit.Actor->bCanStepUpOn)
	{
		return FALSE;
	}
	return TRUE;
}


/**
 * this function will generate a list of points which will be used to probe the ground underneath a ground check
 * this is done by figuring out the size of each check based on MAX_GROUNDCHECK_SIZE, and then building a grid of points
 * with that value.
 * @param TraceStart - center location of spot we need to verify ground position on
 * @param SubdivisionIteration - the current subdivision iteration the expansion is on
 * @param out_Extent - out var for size of extent to use for down line checks
 * @param out_Points - out var for points for down line checks
 * @param out_Dim    - dimension of groundcheck lattice 
 */
#define CHECK_INCREMENT 5.0f
void BuildGroundCheckInfo(const FVector& TraceStart, INT SubdivisionIteration, FLOAT& out_Extent, TArray<FVector>& out_Points, INT& out_Dim)
{
	FLOAT BaseSize = GetStepSize(SubdivisionIteration);

	// determine the size of the check we are going to use	
	INT NewIt = SubdivisionIteration;

	FLOAT TestSize = GetStepSize(NewIt);
	while( TestSize > NAVMESHGEN_MAX_GROUNDCHECK_SIZE )
	{
		TestSize = GetStepSize(++NewIt);
	}

	// now build a list of points to test 
	out_Dim = (INT)((2.0f*BaseSize)/TestSize);
	
	// set cursor to top left of field
	FVector StartPos = TraceStart + FVector(BaseSize,-BaseSize,0.f); // top left
	// now come back to center of top left most segment
	StartPos  += 0.5f*FVector(-TestSize,TestSize,0.f);

	// DEBUG DEBUG DEBUG
//	GWorld->GetWorldInfo()->DrawDebugLine(StartPos,TraceStart,255,255,0,TRUE);

	FVector CurPos(0.f);
	for(INT Idx=0;Idx<out_Dim;++Idx)
	{
		for(INT Jdx=0;Jdx<out_Dim;++Jdx)
		{
			CurPos = StartPos + FVector(-Idx*TestSize,Jdx*TestSize,CHECK_INCREMENT+TestSize);

			// DEBUG DEBUG DEBUG
// 			GWorld->GetWorldInfo()->DrawDebugLine(CurPos,TraceStart,255,0,0,TRUE);
// 			GWorld->GetWorldInfo()->DrawDebugLine(CurPos,CurPos+FVector(0.f,0.f,5.f),255,0,0,TRUE);


			out_Points.AddItem(CurPos);
		}
	}

	out_Extent = (0.5f*TestSize)-1.0f;

}

/**
 * Will return FALSE if there is a drop in any adjacent hit locations of > NAVMESHGEN_MAX_STEP_HEIGHT
 * this is used to determine if we need to run a verfiyslopestep pass on incoming node candidates
 * before passing them off
 * 
 * @param In_Idx - index into TempHits to test step heights vs adjacent points
 * @param Dimension - dimension of temphits array/matrix
 * @param TempHits - array of hits to use for testing
 * @return - TRUE If hits are all within tolerance
 */
UBOOL VerifyDropHeightsToAdjacentPoints(INT In_Idx, INT Dimension, TArray<FCheckResult>& TempHits)
{
	FVector BaseLoc = TempHits(In_Idx).Location;
	
	for(INT OffsetX=-1;OffsetX<2;++OffsetX)
	{
		INT Cur_Idx = In_Idx+(OffsetX*Dimension);
		
		// if we're at either edge of the matrix, skip 
		if( Cur_Idx < 0 || Cur_Idx >= TempHits.Num())
		{
			continue;
		}
		
		INT StartIdx = Cur_Idx;
		for(INT Jdx=-1;Jdx<2;++Jdx)
		{
			Cur_Idx = StartIdx + Jdx;
			// if we are at the edge of this row, skip
			if(Cur_Idx < 0 || Cur_Idx >= TempHits.Num() || Cur_Idx == In_Idx || Abs<INT>((Cur_Idx % Dimension) - (In_Idx % Dimension)) > 1)
			{
				continue;
			}

			if(Abs<FLOAT>(TempHits(Cur_Idx).Location.Z - BaseLoc.Z) > NAVMESHGEN_MAX_STEP_HEIGHT)
			{
				return FALSE;
			}
		}
	}
	return TRUE;
}

/**
 * takes normal samples for each segment from ptcloud origin to hit loc, and averages them
 * 
 * @param Hits - list of hits to calculate a normal for
 * @param PtAvgLoc - average (centroid) of hit locations
 * @return - calculated normal
 */
FVector CalcNormalForHits_CrossPdctToPolyOrigin(const TArray<FCheckResult>& Hits, const FVector& PtAvgLoc)
{
	FVector Result(0.f);
	for(INT Idx=0;Idx<Hits.Num();++Idx)
	{
		FVector CurVec = (Hits(Idx).Location - PtAvgLoc).SafeNormal();
		FVector Horiz_Cross_Vec = CurVec ^ FVector(0.f,0.f,1.f);

		Result += Horiz_Cross_Vec ^ CurVec;
	}

	return Result.SafeNormal();
}

/**
 * this will construct quads at each group of points and compute a normal for that quad, and average all quads together
 * 
 * @param Hits - list of hits to calculate a normal for
 * @param PtAvgLoc - average (centroid) of hit locations
 * @param Dimensions - dimension of hit matrix
 * @return - calculated normal
 */
FVector CalcNormalForHits_QuadNormAvg(const TArray<FCheckResult>& Hits, const FVector& PtAvgLoc, INT Dimensions)
{
	FVector Result(0.f);
	for(INT Idx=0;Idx<Hits.Num();Idx+=2)
	{
		FVector TopLeft = Hits(Idx).Location;
		
		if((Idx+1) >= Hits.Num() || (Idx+1) % Dimensions < (Idx%Dimensions))
		{
			continue; // we're on the right edge
		}
		FVector TopRight = Hits(Idx+1).Location;
		
		if(Idx+Dimensions >= Hits.Num())
		{
			break; // we're on the bottom row, break out of the loop
		}
		FVector BottomLeft = Hits(Idx+Dimensions).Location;
		FVector BottomRight = Hits(Idx+Dimensions+1).Location;		

		// tri1
		Result += (BottomLeft-TopRight)^(TopLeft-TopRight);
		// tri2
		Result += (TopLeft-BottomLeft)^(BottomRight-BottomLeft);
	}

	return Result.SafeNormal();
}

/**
 * given a list of hit information, calculate a suitable ground normal
 * @param Hits - array of hits to use for ground normal calc
 * @param PtAvgLoc - centroid (average) of hit locations
 * @param Dimensions - dimension of hit array matrix
 * @return calculated normal
 */
FVector CalcNormalForHits(const TArray<FCheckResult>& Hits, const FVector& PtAvgLoc, INT Dimensions)
{
	static INT Method=1;
	switch(Method)
	{
	case 0:
		return CalcNormalForHits_CrossPdctToPolyOrigin(Hits,PtAvgLoc);
	case 1:
		return CalcNormalForHits_QuadNormAvg(Hits,PtAvgLoc,Dimensions);
	default:
		return FVector(0.f);
	}
}

/**
 * returns whether or not this pylon should be built right now.. if FALSE this pylon will not rebuilt during this navmesh generation pass
 * @param bOnlyBuildSelected - the value of 'only build selected' coming from the editor
 * @return - TRUE if this pylon should be wiped and rebuilt
 */
UBOOL APylon::ShouldBuildThisPylon(UBOOL bOnlyBuildSelected)
{
	return !bOnlyBuildSelected || IsSelected();
}


/**
 * this function will slide a box downward from a raised position until a position which is non-colliding, then multiple raycasts downward
 * will be performed to ascertain the topology of the ground underneath the ground check.  This gives us a valid position for the ground at a given 
 * expansion point, as well as valid normal data for the extent being swept downward
 * @param TestBasePos - position to start testing from
 * @param Result      - hit result describing information about the ground position found
 * @param Scout       - scout instance to be used during this ground check
 * @param SubdivisionIteration - the number of times to default stepsize has been subdivided for the current callchain
 * @param out_bNeedsSlopeCheck - if a large gap is found while performing the second stage ground check, this will be turned on
 *                               indicating a call to 'VerifySlopeStep' is necessary
 * @return - TRUE if a valid ground posiiton was found
 */
UBOOL APylon::FindGround(const FVector& TestBasePos, FCheckResult& Result,AScout* Scout, INT SubdivisionIteration,UBOOL* out_bNeedsSlopeCheck/*=NULL*/)
{

	static FLOAT DropBuf = 5.0f;	
#define DROP_HEIGHT_BUFFER DropBuf

	FLOAT StepSize = GetStepSize(SubdivisionIteration);
	FVector Extent = FVector(StepSize);
	Extent.Z = NAVMESHGEN_ENTITY_HALF_HEIGHT;
	
	FLOAT MaxSlopeZ = MaxStepForSlope(2.0f * StepSize);
	FLOAT StartingHeightOffset= MaxSlopeZ + 2.0f*NAVMESHGEN_STARTING_HEIGHT_OFFSET + DROP_HEIGHT_BUFFER;
#define MAX_POINTCHECK_SWEEP_DIST NAVMESHGEN_MAX_DROP_HEIGHT+StartingHeightOffset

	// put the cursor at the highest possible point plus a small buffer and find an open spot
	FVector CurTestLoc = TestBasePos + FVector(0.f,0.f,StartingHeightOffset);

	UBOOL bHitSomethingEver = FALSE;

	FLOAT Dist = 0.f;
	while(Dist < MAX_POINTCHECK_SWEEP_DIST)
	{
		// test current position to see if it's clear
		FMemMark UpMark(GMainThreadMemStack);
		FCheckResult* UpHits = GWorld->MultiPointCheck(GMainThreadMemStack,CurTestLoc,Extent,EXPANSION_TRACE_FLAGS);
		UBOOL bHit = FALSE;
		for( FCheckResult* Hit = UpHits; Hit; Hit = Hit->GetNext() )
		{
			if(!Hit->Actor->IsA(ANavigationPoint::StaticClass()))
			{
				bHit = TRUE;
				break;
			}
		}

		// if we didn't hit anything try against movers (wackiness because pointchecks don't take a source actor)
		if(!bHit)
		{
			FCheckResult* UpHits = GWorld->MultiPointCheck(GMainThreadMemStack,CurTestLoc,Extent,(EXPANSION_TRACE_FLAGS)^TRACE_Blocking);
			for( FCheckResult* Hit = UpHits; Hit; Hit = Hit->GetNext() )
			{
				if(Hit->Actor->IsA(AInterpActor::StaticClass()))
				{
					bHit = TRUE;
					break;
				}
			}
		}
		UpMark.Pop();


		// if we're still in the ceiling, keep moving downward till we find open space
		if(bHit)
		{
			CurTestLoc.Z -= CHECK_INCREMENT;
			Dist += CHECK_INCREMENT;			
		}
		else 
		{
			// otherwise, we're clear! use this spot (do trace down to get hit info, and snap exactly to surface)
			FVector TraceStart = CurTestLoc;
			FVector TraceEnd = TraceStart + FVector(0.f,0.f,-CHECK_INCREMENT*2.0f);
					
			// determine how we're going to split up this step for ground checks
			TArray<FVector> Points;
			FLOAT TestExtent(0.f);
			INT MeshDimension=0;
			BuildGroundCheckInfo(TraceStart, SubdivisionIteration, TestExtent, Points,MeshDimension);

			TArray<FCheckResult> TempHits;
			TempHits.AddZeroed(Points.Num());

			// run the ground checks
			FVector Avg(0.f);

			FLOAT HeightOffset = 2.0f*NAVMESHGEN_ENTITY_HALF_HEIGHT + NAVMESHGEN_MAX_DROP_HEIGHT+CHECK_INCREMENT+TestExtent+MaxSlopeZ;
			for(INT Idx=0;Idx<Points.Num();++Idx)
			{
				if(GWorld->SingleLineCheck(TempHits(Idx),Scout,Points(Idx)+FVector(0.f,0.f,-HeightOffset),Points(Idx),EXPANSION_TRACE_FLAGS,FVector(TestExtent)))
				{	
//					GWorld->GetWorldInfo()->DrawDebugLine(Points(Idx)+FVector(0.f,0.f,-HeightOffset),Points(Idx),255,255,0,TRUE);
					return FALSE;
				}
				else if( ! Scout->NavMeshGen_IsValidGroundHit(TempHits(Idx)))
				{
					return FALSE;
				}
				else if(TempHits(Idx).bStartPenetrating || TempHits(Idx).Time < KINDA_SMALL_NUMBER)
				{
//					GWorld->GetWorldInfo()->DrawDebugLine(Points(Idx)+FVector(0.f,0.f,-HeightOffset),Points(Idx),255,0,255,TRUE);
					return FALSE;
				}
				Avg+=TempHits(Idx).Location;
			}

			Result.Location = Avg/(FLOAT)Points.Num();

			// go through the ground positions 4 at a time checking for large steps which will require a slope verification run
			for(INT Idx=0;Idx<TempHits.Num();++Idx)
			{
				if(out_bNeedsSlopeCheck != NULL && !VerifyDropHeightsToAdjacentPoints(Idx, MeshDimension,TempHits))
				{	
					*out_bNeedsSlopeCheck=TRUE;
				}
			}

			Result.Normal = CalcNormalForHits(TempHits,Result.Location,MeshDimension);
			Result.Location.Z -= TestExtent;
			Result.Actor = TempHits(0).Actor;

			return TRUE;
		}


	}

	// if we got here means we never found a spot.. indicate as such
	return FALSE;
}


/**
 * will sweep up from ground position and find the maximum supporting height of a new poly
 * 
 * @param TestBasePos - base position of node to test ceiling height for
 * @param Result	  - resulting hit 
 * @param Scout		  - scout to use for params/tests
 * @param Up		  - upward direction for this poly
 * @param Extent	  - extent to use for linechecks
 * @return - location of ceiling found
 */
FVector APylon::FindCeiling(const FVector& TestBasePos, FCheckResult& Result,AScout* Scout, const FVector& Up, const FVector& Extent)
{

	checkSlowish(appIsNearlyEqual(Up.Size(),1.f,float(KINDA_SMALL_NUMBER)));
	FVector ModExtent = Extent;
	ModExtent.Z = 5.f;

	FVector TraceStart = TestBasePos + Up * ModExtent.Z;
	FVector TraceEnd = TraceStart + Up * NAVMESHGEN_MAX_POLY_HEIGHT_PYLON(this);


	// first trace down to find the first position that can handle this extent
	FCheckResult Hit(1.f);
	if(!GWorld->SingleLineCheck(Hit,Scout,TraceStart,TraceStart+(Up*Extent.X),EXPANSION_TRACE_FLAGS,ModExtent))
	{
		TraceStart = Hit.Location;
	}

	// trace up and find the ceiling for this poly
	FMemMark UpMark(GMainThreadMemStack);
	FCheckResult* UpHits = GWorld->MultiLineCheck(GMainThreadMemStack,TraceEnd,TraceStart,ModExtent,EXPANSION_TRACE_FLAGS,Scout);
	UBOOL bHit = FALSE;
	for( FCheckResult* Hit = UpHits; Hit; Hit = Hit->GetNext() )
	{
		if(Hit->Time > 0.01f && Hit->bStartPenetrating == FALSE)
		{
			Result = *Hit;
			bHit = TRUE;
			break;
		}
	}
	UpMark.Pop();

	// if we didn't hit anything, return the maximum
	if(!bHit)
	{
		return TraceEnd;
	}
	else
	{
		FVector ReturnVal = Result.Location + Up * Min<FLOAT>(NAVMESHGEN_MAX_POLY_HEIGHT_PYLON(this)+5.0f,(ModExtent.Z*0.5f));
		// otherwise return the locaiton we hit against (push up half extent to account for trace stopping at extent loc)
		return ReturnVal;
	}	
}

/**
 * takes desired offset , and scales it up to fit a box around the offset start (e.g. expand in a grid)
 *
 * @param BaseOffset - non grid-aligned offset we want to find a grid-aligned version of
 * @return - modified offset
 */
FVector GetExpansionStepSize(const FVector& BaseOffset)
{
	FVector OffsetNormalized = BaseOffset.SafeNormal();
	// find the closest cardinal direction to our offset
	FLOAT DotP[4] = {OffsetNormalized | FVector(1.f,0.f,0.f),//north
		OffsetNormalized | FVector(-1.f,0.f,0.f),//south
		OffsetNormalized | FVector(0.f,-1.f,0.f),//west	
		OffsetNormalized | FVector(0.f,1.f,0.f)};//east
	FLOAT DotPBest = 0.f;
	for(INT Idx=0;Idx<4;Idx++){ DotPBest = Max<FLOAT>(DotPBest,DotP[Idx]); }

	return BaseOffset/DotPBest;
}

/**
 * this function will return the height of a poly adjacent to parentpoly as if it were along a surface made of the plane of the poly
 *
 * @param ParentPoly - poly to check slope from
 * @param DestPolyLoc - test location to see determine height for along parent poly's plane
 * @return - height of new position 
 */
FLOAT ProjectHeightFromPolyNorm(FNavMeshPolyBase* ParentPoly, const FVector& DestPolyLoc)
{
	FVector PolyCtr = ParentPoly->NavMesh->L2WTransformFVector(ParentPoly->CalcCenter());
	FVector PolyNorm = ParentPoly->NavMesh->L2WTransformNormal(ParentPoly->CalcNormal());

	FVector CtrToNewLoc = (DestPolyLoc - PolyCtr); 
	FVector Perp =  CtrToNewLoc ^ PolyNorm;
	FVector ProjDir = (Perp ^ PolyNorm).SafeNormal();

	FVector NewPos = PolyCtr + (ProjDir * (ProjDir | CtrToNewLoc));
	//GWorld->GetWorldInfo()->DrawDebugLine(DestPolyLoc,NewPos,255,255,255,TRUE);
	//GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(NewPos,FRotator(0,0,0),10.f,TRUE);
	return NewPos.Z;
}

/**
 * sweeps along the given trajectory testing every STEPHEIGHT_TEST_STEP_SIZE units to check for dropoffs that exceed max step height
 *
 * @param Start - start of sweep (should be at centroid or ground + ENTITY_HALF_HEIGHT)
 * @param Dir   - direction of sweep
 * @param SweepDistance - distance to sweep in Dir direction
 * @param Scout - scout to use for params/tests
 * @Param out_ZDelta - out param of discovered height delta
 * @param MaxHeight  - maximum acceptable height offset
 * @param ExtentOverride - override for extent to use for downchecks
 * @return - TRUE Of trajectory is valid
 */
#define STEPHEIGHT_TEST_STEP_SIZE 5.f
#define VerifyStepAlongTrajectory_DEBUG 0
UBOOL VerifyStepAlongTrajectory(const FVector& Start, const FVector& Dir, FLOAT SweepDistance, AScout* Scout, FLOAT& out_ZDelta, FLOAT MaxHeightOffset, FVector* ExtentOverride=NULL)
{

	FVector Extent = (ExtentOverride != NULL)? FVector(*ExtentOverride) :  FVector(STEPHEIGHT_TEST_STEP_SIZE);
	// make sure we move at least stepsize horizontally each time
	FLOAT CompensatedStepSize = STEPHEIGHT_TEST_STEP_SIZE / (Dir|Dir.SafeNormal2D());

	FVector DownOffset = FVector(0.f,0.f,-MaxStepForSlope(SweepDistance));

	// now do linechecks down periodically and make sure that we don't exceed max step height at each point
	FLOAT CurDist = 0.f;

	// ** find sane starting position (have to do trace here because the parent poly could already be floating some)
	FCheckResult TempHit(1.f);
	FVector CurTestStartPos = Start;
	FVector End = Start + Dir * SweepDistance;

	CurTestStartPos.Z -= NAVMESHGEN_ENTITY_HALF_HEIGHT - STEPHEIGHT_TEST_STEP_SIZE;
	FVector InitialDownTestStart = CurTestStartPos+FVector(0.f,0.f,NAVMESHGEN_MAX_STEP_HEIGHT);
	FVector InitialDownTestEnd = CurTestStartPos+DownOffset;
	if(!GWorld->SingleLineCheck(TempHit,Scout,InitialDownTestEnd,InitialDownTestStart,EXPANSION_TRACE_FLAGS,Extent))
	{
		CurTestStartPos = TempHit.Location;

		FVector DEBUGSTARTPOS = CurTestStartPos;

		FLOAT PrevZ = CurTestStartPos.Z;

#define	LATERAL_EXTENT_DIFFERENCE 1.0f

		FVector LateralExtent = FVector( Max<FLOAT>(0.1f,Extent.X-LATERAL_EXTENT_DIFFERENCE), Max<FLOAT>(0.1f,Extent.Y-LATERAL_EXTENT_DIFFERENCE), Max<FLOAT>(0.1f,Extent.Z-LATERAL_EXTENT_DIFFERENCE) );

		FVector PrevPos = CurTestStartPos;
		while(CurDist <= SweepDistance)
		{

			// trace laterally first to find starting position
			if(CurDist > 0.f && !GWorld->SingleLineCheck(TempHit,Scout,CurTestStartPos+FVector(0.f,0.f,1.f),PrevPos+FVector(0.f,0.f,1.f),EXPANSION_TRACE_FLAGS,LateralExtent))
			{
				if(TempHit.bStartPenetrating || TempHit.Time < KINDA_SMALL_NUMBER)
				{
#if(VerifyStepAlongTrajectory_DEBUG)
					GWorld->GetWorldInfo()->DrawDebugBox(CurTestStartPos,FVector(STEPHEIGHT_TEST_STEP_SIZE),255,0,255,TRUE);
					GWorld->GetWorldInfo()->DrawDebugBox(TempHit.Location,FVector(STEPHEIGHT_TEST_STEP_SIZE),255,255,255,TRUE); 				
#endif
					return FALSE;
				}
				
				// lateral trace is slightly smaller, so pull back by that amount to get a clear position for down checks
				FVector TraceDir = ((CurTestStartPos+FVector(0.f,0.f,1.f))-(PrevPos+FVector(0.f,0.f,1.f))).SafeNormal();
				// set location to the place we just hit
				CurTestStartPos = TempHit.Location - GetExpansionStepSize(TraceDir*LATERAL_EXTENT_DIFFERENCE);

				// adjust curdist to compensate for the early hit
				CurDist -= CompensatedStepSize;
				CurDist += (PrevPos-CurTestStartPos).Size();
			}

			// if we didn't hit anything, or if the deltaZ is too big
			FVector Cp(0.f);
			if(GWorld->SingleLineCheck(TempHit,Scout,CurTestStartPos+DownOffset,CurTestStartPos+FVector(0.f,0.f,NAVMESHGEN_MAX_STEP_HEIGHT+NAVMESHGEN_ENTITY_HALF_HEIGHT),EXPANSION_TRACE_FLAGS,Extent) 
				|| Abs<FLOAT>(TempHit.Location.Z - PrevZ) > NAVMESHGEN_MAX_STEP_HEIGHT
				|| TempHit.Normal.Z < Scout->WalkableFloorZ 
				|| PointDistToSegment( TempHit.Location, Start,End,Cp) > NAVMESHGEN_MAX_STEP_HEIGHT+MaxHeightOffset )
			{
				if(TempHit.bStartPenetrating || TempHit.Time < KINDA_SMALL_NUMBER)
				{
//					GWorld->GetWorldInfo()->DrawDebugBox(TempHit.Location,FVector(STEPHEIGHT_TEST_STEP_SIZE),255,255,0,TRUE);
				}
				// DEBUG
#if(VerifyStepAlongTrajectory_DEBUG)
 				GWorld->GetWorldInfo()->DrawDebugBox(DEBUGSTARTPOS,FVector(STEPHEIGHT_TEST_STEP_SIZE),255,255,0,TRUE);
 				GWorld->GetWorldInfo()->DrawDebugLine(CurTestStartPos,TempHit.Location,255,0,0,TRUE);
 				GWorld->GetWorldInfo()->DrawDebugBox(TempHit.Location,FVector(STEPHEIGHT_TEST_STEP_SIZE),255,(TempHit.Actor)?0:255,0,TRUE);
				GWorld->GetWorldInfo()->DrawDebugLine(DEBUGSTARTPOS,TempHit.Location,255,0,0,TRUE);
				GWorld->GetWorldInfo()->DrawDebugLine(Start,End,255,0,255,TRUE);
#endif

				out_ZDelta = TempHit.Location.Z - PrevZ;
				return FALSE; 
			}
			else
			{
				// DEBUG
// #if(VerifyStepAlongTrajectory_DEBUG)
//    				GWorld->GetWorldInfo()->DrawDebugLine(CurTestStartPos,TempHit.Location,0,255,0,TRUE);
//    				GWorld->GetWorldInfo()->DrawDebugBox(TempHit.Location,FVector(STEPHEIGHT_TEST_STEP_SIZE),0,255,0,TRUE);
// #endif

				PrevZ = TempHit.Location.Z;
				CurDist += CompensatedStepSize;

				PrevPos=CurTestStartPos;
				CurTestStartPos.Z = PrevZ;			
				CurTestStartPos += Dir * CompensatedStepSize;
			}
		}
	}
	else
	{		
		// this means that the poly was floating over nothing, or whatever it is floating over is far above what's below
// 		GWorld->GetWorldInfo()->DrawDebugLine(InitialDownTestEnd,InitialDownTestStart,255,0,0,TRUE);
// 		GWorld->GetWorldInfo()->DrawDebugLine(Start,End,255,128,0,TRUE);
		out_ZDelta = -(NAVMESHGEN_MAX_DROP_HEIGHT+1.f);
		return FALSE;
	}

	return TRUE;
}


/**
 * when zdelta is greater than step size, but less than the height change due to allowable slopes,
 * this function is called ot do extra verification (to determine if it is just a slope, or if there is a big step)
 * 
 * @param Scout - scout to use for tests/params
 * @param NewNodePos - new position of node we need to test step for 
 * @param CurrNodePosWithHeightOffset - predecessor node's current position (with height offset from ground0
 * @param StepSize - current step size (size of polys being added right now)
 * @param out_ZDelta - out value of any ZDelta found
 * @param ParentPoly - the parent (predecessor) Poly we're veryfing a step from
 * @return - TRUE If step is valid
 */
UBOOL APylon::VerifySlopeStep(AScout* Scout,
							  const FVector& NewNodePos,
							  const FVector& CurrNodePosWithHeightOffset,
							  FLOAT StepSize,
							  FLOAT& out_ZDelta,
							  FNavMeshPolyBase* ParentPoly)
{
	
	FLOAT HeightOffset = Max<FLOAT>(NAVMESHGEN_ENTITY_HALF_HEIGHT,MaxStepForSlope(2.0f*StepSize));
	FLOAT ParentStepSize = (ParentPoly->GetVertLocation(0,LOCAL_SPACE) - ParentPoly->GetVertLocation(1,LOCAL_SPACE)).Size() * 0.5f;

	// snap to appropriate step size
	if (ParentStepSize > NAVMESHGEN_STEP_SIZE)
	{
		ParentStepSize=NAVMESHGEN_STEP_SIZE;
	}
	else if(ParentStepSize<NAVMESHGEN_STEP_SIZE)
	{
		for(INT SubDivisionIteration=1;SubDivisionIteration<=NAVMESHGEN_MAX_SUBDIVISIONS;++SubDivisionIteration)
		{
			if( ParentStepSize > GetStepSize(SubDivisionIteration) )
			{
				ParentStepSize = GetStepSize(SubDivisionIteration);
				break;
			}
		}
	}

	FVector BaseDir = (NewNodePos - CurrNodePosWithHeightOffset).SafeNormal();
	

	// *** do three sweeps, starting down the center, then one on either side 
	

	// >>>> CENTER

	//	for the center, we want to go from the edge of the parent poly furthest away from the new poly, all the way to the edge of the new poly
	FVector StartPos = CurrNodePosWithHeightOffset - GetExpansionStepSize(BaseDir*(ParentStepSize-STEPHEIGHT_TEST_STEP_SIZE));
	FVector EndPos = NewNodePos + GetExpansionStepSize(BaseDir*(StepSize-STEPHEIGHT_TEST_STEP_SIZE)); 
	FLOAT DistToNewPos = (EndPos - StartPos).Size();
	FLOAT SweepDist = DistToNewPos;
	
	FVector CurTestStartPos = StartPos ;

	if( ! VerifyStepAlongTrajectory(CurTestStartPos,BaseDir,SweepDist,Scout,out_ZDelta,HeightOffset))
	{
		return FALSE;
	}
	// <<<<

	// only do side tests if our step size is sufficiently bigger than the extent of the checks we would be doing
	if(STEPHEIGHT_TEST_STEP_SIZE/StepSize<0.5f)
	{
		// ** SIDES
		FLOAT FwdDotAbs = Abs<FLOAT>((FVector(BaseDir.X,BaseDir.Y,0.f) | FVector(1.f,0.f,0.f)));
		UBOOL bDiagStep = !appIsNearlyEqual((FLOAT)FwdDotAbs,1.f,(FLOAT)KINDA_SMALL_NUMBER) && !appIsNearlyEqual((FLOAT)FwdDotAbs,0.f,(FLOAT)KINDA_SMALL_NUMBER);

		// figure out how far off center we want to go laterally 
		FVector Perp = BaseDir ^ FVector(0.f,0.f,1.f);
		FVector ParentPerpOffset = GetExpansionStepSize(Perp*(ParentStepSize-STEPHEIGHT_TEST_STEP_SIZE))-1.0f;	
		FVector NextStepPerpOffset = GetExpansionStepSize(Perp*(StepSize-STEPHEIGHT_TEST_STEP_SIZE));

		// >>>> SIDE ONE	
		//     Starting position
		CurTestStartPos = CurrNodePosWithHeightOffset + ParentPerpOffset;
		CurTestStartPos.Z = CurrNodePosWithHeightOffset.Z;
		// if this is not a diagonal step, move backwards to the rear edge of the parent poly
		if(!bDiagStep)
		{
			CurTestStartPos -= (ParentStepSize-STEPHEIGHT_TEST_STEP_SIZE) * BaseDir;
		}
		//     End position
		FVector EndPt = NewNodePos + NextStepPerpOffset;
		if(!bDiagStep)
		{
			EndPt += (StepSize-STEPHEIGHT_TEST_STEP_SIZE) * BaseDir;
		}

		FVector Delta = (EndPt-CurTestStartPos);
		SweepDist = Delta.Size();
		FVector Dir = Delta/SweepDist;

		if( ! VerifyStepAlongTrajectory(CurTestStartPos,Dir,SweepDist,Scout,out_ZDelta,HeightOffset))
		{
			return FALSE;
		}

		// >>>> SIDE TWO 
		//     Starting position
		CurTestStartPos = CurrNodePosWithHeightOffset - ParentPerpOffset;
		CurTestStartPos.Z = CurrNodePosWithHeightOffset.Z;
		// if this is not a diagonal step, move backwards to the rear edge of the parent poly
		if(!bDiagStep)
		{
			CurTestStartPos -= (ParentStepSize-STEPHEIGHT_TEST_STEP_SIZE) * BaseDir;
		}
		//     End position
		EndPt = NewNodePos - NextStepPerpOffset;
		if(!bDiagStep)
		{
			EndPt += (StepSize-STEPHEIGHT_TEST_STEP_SIZE) * BaseDir;
		}

		Delta = (EndPt - CurTestStartPos);
		SweepDist = Delta.Size();
		Dir = Delta/SweepDist;

		if( ! VerifyStepAlongTrajectory(CurTestStartPos,Dir,SweepDist,Scout,out_ZDelta,HeightOffset))
		{
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * tries to find ground position below passed location and calls AddNewNode if there is nothing in the way of expansion, and the ground isn't too steep/etc..
 * 
 * @param NewNodeTestLocation		  - location to try and add a new node 
 * @param CurrNodePosWithHeightOffset - predecessor node's position (offset from ground with height offset)
 * @param CUrrNodePos				  - predecessor node's position without height offset
 * @param Hit						  - output struct indicating values associated with the ground hit
 * @param Scout						  - Scout to use for tests
 * @param out_bIvalidSpot			  - out var indicating this spot was invalid due to out of bounds/something else
 * @param SubdivisionIteration        - the current subdivision iteration
 * @param bDiag						  - are we checking ground positiopn for a diagonal expansion
 * @param ParentPoly				  - optional predecessor polygon we're expanding from
 * @return - the new poly added if any
 */
FNavMeshPolyBase* APylon::ConditionalAddNodeHere(const FVector& NewNodeTestLocation,
												 const FVector& CurrNodePosWithHeightOffset,
												 const FVector& CurrNodePos,
												 FCheckResult& Hit,
												 AScout* Scout, 
												 UBOOL& out_bInvalidSpot,
												 INT SubdivisionIteration,
												 UBOOL bDiag,
												 FNavMeshPolyBase* ParentPoly)
{

	FLOAT StepSize = GetStepSize(SubdivisionIteration);

	FVector PylonPosition = Location;
	FVector FindGroundStartPos = NewNodeTestLocation;
	FindGroundStartPos.Z = CurrNodePos.Z;
	UBOOL bNeedsSlopeVerify=FALSE;
	if(FindGround(FindGroundStartPos,Hit,Scout,SubdivisionIteration,&bNeedsSlopeVerify))
	{
		FCheckResult NewHit(1.f);

		FVector Extent = FVector(StepSize);

		FVector NewNodePos = Hit.Location+FVector(0.f,0.f,NAVMESHGEN_ENTITY_HALF_HEIGHT);

		FLOAT LineCheckHeightOffset = Max<FLOAT>(NAVMESHGEN_ENTITY_HALF_HEIGHT,StepSize*2.0f);
		FVector LateralLineCheckStart = CurrNodePos+FVector(0.f,0.f,LineCheckHeightOffset);
		FVector LateralLineCheckEnd = Hit.Location+FVector(0.f,0.f,LineCheckHeightOffset);
		
		UBOOL bLateralLineCheckClear = GWorld->SingleLineCheck(NewHit,Scout, LateralLineCheckEnd,LateralLineCheckStart, EXPANSION_TRACE_FLAGS,Extent);

		FLOAT SlopeMaxZDelta = MaxStepForSlope(2.0f*StepSize);

		if(Hit.bStartPenetrating || Hit.Actor == NULL)
		{
			//GWorld->GetWorldInfo()->DrawDebugLine(Hit.Location+FVector(0.f,0.f,NAVMESHGEN_ENTITY_HALF_HEIGHT),CurrNodePosWithHeightOffset,0,0,255,TRUE);
		}
		else
		{

			FVector HitLocation = Hit.Location;
			FVector Norm = Hit.Normal;

			// if we're subdividing, use the parent's height so we don't get height discrepancies along edges
			if(ParentPoly != NULL && SubdivisionIteration > 0 && !NAVMESHGEN_DISABLE_SUBDIVISION_HEIGHT_SNAPPING)
			{
				HitLocation.Z = ProjectHeightFromPolyNorm(ParentPoly,Hit.Location);
				FLOAT AbsDeltaFromNewPos = Abs<FLOAT>(HitLocation.Z - Hit.Location.Z);
				if(AbsDeltaFromNewPos > NAVMESHGEN_MAX_STEP_HEIGHT)
				{
					HitLocation = Hit.Location;
				}
				Norm = ParentPoly->GetPolyNormal();
			}

			UBOOL bValidSurfaceNorm =  (Hit.Normal.Z >= Scout->WalkableFloorZ);
			UBOOL bAddNode = FALSE;
			FLOAT ZDelta = HitLocation.Z - CurrNodePos.Z;
			FLOAT AbsZDelta = Abs<FLOAT>(ZDelta);
			if( bValidSurfaceNorm && AbsZDelta < NAVMESHGEN_MAX_STEP_HEIGHT && !bNeedsSlopeVerify) 
			{
				if(bLateralLineCheckClear)
				{
					bAddNode=TRUE;
				}
			}
			// if the zdelta is less than what we would get from the maximum allowed slope
			// do line checks to verify we never exceed maxstepheight along the slope
			else if(bLateralLineCheckClear && AbsZDelta < SlopeMaxZDelta)
			{
				bAddNode = VerifySlopeStep(Scout,Hit.Location+FVector(0.f,0.f,NAVMESHGEN_ENTITY_HALF_HEIGHT),CurrNodePosWithHeightOffset,StepSize,ZDelta,ParentPoly);
// 				if(!bAddNode)
// 				{
// 					GWorld->GetWorldInfo()->DrawDebugLine(HitLocation,HitLocation+FVector(0,0,50),255,255,0,TRUE);
// 				}
			}
			else if(bLateralLineCheckClear)
			{
				//GWorld->GetWorldInfo()->DrawDebugLine(HitLocation,FindGroundStartPos,0,0,255,TRUE);
			}

// 			if(!bLateralLineCheckClear)
// 			{
// 				GWorld->GetWorldInfo()->DrawDebugLine(NewHit.Location,LateralLineCheckStart,0,255,255,TRUE);
// 				GWorld->GetWorldInfo()->DrawDebugBox(NewHit.Location,Extent,0,255,255,TRUE);
// 			}
			
			if( !bAddNode )
			{
				if( !bDiag && SubdivisionIteration == 0 )
				{

					HandleFailedAddNode( Scout, CurrNodePos, FindGroundStartPos );

					// skip pullback when this was not a steep slope detect up front
					SavePossibleDropDownEdge(NewNodePos,CurrNodePos,Hit.Normal,ParentPoly,bAddNode);
				}
			}
			else			
			if(bAddNode)
			{
				// if this is a subdivided poly do one extra linecheck to make sure this poly isn't inside of something
				if(SubdivisionIteration > 0)
				{
					FLOAT SlopeOffset = 2.0f*StepSize*appTan(appAcos(Norm.Z)) + StepSize; // +stepside to account for extent
					FVector BasePos = HitLocation + FVector(0.f,0.f,SlopeOffset);
					FCheckResult FinalHit(1.f);
					if(!GWorld->SingleLineCheck(FinalHit,Scout,BasePos,BasePos+FVector(0.f,0.f,StepSize),EXPANSION_TRACE_FLAGS,Extent))
					{
						FVector GroundLoc = FinalHit.Location - FVector(0.f,0.f,Extent.Z);
						if( FinalHit.Time > 0.f && !FinalHit.bStartPenetrating && Abs<FLOAT>(GroundLoc.Z - HitLocation.Z) < SlopeOffset) 
						{
							HitLocation = GroundLoc;
						}
						else
						{
							//GWorld->GetWorldInfo()->DrawDebugBox(FinalHit.Location,Extent,255,128,0,TRUE);
							return NULL;
						}
					}
					else
					{
						//GWorld->GetWorldInfo()->DrawDebugBox(BasePos,Extent,0,255,0,TRUE);
					}
				}
				// since they're within stepheight of each other add in both directions
				FNavMeshPolyBase* Poly = AddNewNode(HitLocation,Norm,&out_bInvalidSpot,SubdivisionIteration);
				if(NAVMESHGEN_DRAW_POLY_PARENTS && Poly != NULL)
				{
					if(AbsZDelta < NAVMESHGEN_MAX_STEP_HEIGHT)
					{
						GWorld->GetWorldInfo()->DrawDebugLine(HitLocation,CurrNodePos,0,255,255,TRUE);
					}
					else
					{
						GWorld->GetWorldInfo()->DrawDebugLine(HitLocation,CurrNodePos,0,255,0,TRUE);
					}
				}

				// if poly is NULL and invalidspot is false, that means we're intersecting the mesh, but at a different stepheight.. which 
				// means we may have a good spot for a dropdown edge
				if( Poly==NULL && out_bInvalidSpot==FALSE )
				{
					if(!bDiag && SubdivisionIteration == 0)
					{
						HandleFailedAddNode( Scout, CurrNodePos, FindGroundStartPos );

						// skip pullback when this was not a steep slope detect up front
						SavePossibleDropDownEdge(NewNodePos,CurrNodePos,Hit.Normal,ParentPoly,bAddNode);
					}
				}
				return Poly;
			}
		}
	}
	else
	{
		//GWorld->GetWorldInfo()->DrawDebugLine(FindGroundStartPos,FindGroundStartPos+FVector(0,0,50),255,0,255,TRUE);
		if( !bDiag && SubdivisionIteration == 0 )
		{
// 			GWorld->GetWorldInfo()->DrawDebugLine(CurrNodePos, FindGroundStartPos,255,0,255,TRUE);
// 			GWorld->GetWorldInfo()->DrawDebugBox( CurrNodePos, FVector(2.f), 255, 0, 0, TRUE );
			HandleFailedAddNode( Scout, CurrNodePos, FindGroundStartPos );
		}
	}


	return NULL;
}

/**
 *	Attempts to find an edge (exactly) shared by Poly1 and Poly2 (e.g. an edge which shares the same vertIDs on both polys)
 *  used during convex poly-poly merging
 *  @param Poly1 - first poly to look for shared edge in
 *  @param Poly2  - second poly to look for shared edge in 
 *  @param NavMesh - mesh which contains the polys in question
 *  @param out_Vert1 - output param populated with the vertex ID of the first vert in the shared edge
 *  @param out_Vert2 - output param populated with the vertex ID of the second vert in the shared edge
 * @return -TRUE if a shared edge was found
 */
extern INT Increment(INT OldIdx,INT Direction, INT NumElements);
UBOOL FindSharedEdge(FNavMeshPolyBase* Poly1, FNavMeshPolyBase* Poly2, UNavigationMeshBase* NavMesh, VERTID& out_Vert1,VERTID& out_Vert2)
{
	// for each vert in poly 1
	// walk the first poly, find edges that match in the second 
	for(INT Idx=0;Idx<Poly1->PolyVerts.Num();++Idx)
	{
		VERTID Cur_VertIdx = Poly1->PolyVerts(Idx);
		VERTID Next_VertIdx = Poly1->PolyVerts(Increment(Idx,1,Poly1->PolyVerts.Num()));

		INT OtherPolyVertIdx=-1;
		if(Poly2->PolyVerts.FindItem(Cur_VertIdx,OtherPolyVertIdx))
		{
			// if this vert is in the other poly, check to see if prev is our next.. if so we have a shared edge!
			VERTID Other_PrevIdx = Poly2->PolyVerts(Increment(OtherPolyVertIdx,-1,Poly2->PolyVerts.Num()));
			if( Other_PrevIdx == Next_VertIdx )
			{
				out_Vert1 = Cur_VertIdx;
				out_Vert2 = Next_VertIdx;
				return TRUE;
			}
		}
	}

	return FALSE;

}



/**
 *	Internal (recursively called) function which will attempt to subdivide an attempted expansion step by halfing the check size each time
 *  @param ParentPoly - poly we're expending from
 *  @param NewNodeTestLocation - the location we're attempting to expand to
 *  @param CurrNodePosWithHeightOffset - location of predecessor poly with adjusted for height (e.g. pushed up from the ground)
 *  @param CurrNodePos                 - location (on ground) of predecessor poly
 *  @param Hit						   - output hit info storing information about any geometry we encountered
 *  @param Scout					   - ref to scout we are using for parameters
 *  @param AddedPolys				   - output array of polys which this function added
 *  @param bDiag					   - TRUE if this is a diagonal (e.g. non cardinal) expansion from the predecessor poly
 *  @param SubdivisionIteration        - iteration of subdivision we're on (e.g. how many times have we halved expansion size)
 *  @return - number of polys added
 */
INT APylon::SubdivideExpandInternal( FNavMeshPolyBase* ParentPoly, 
									const FVector& NewNodeTestLocation,
									const FVector& CurrNodePosWithHeightOffset,
									const FVector& CurrNodePos,
									FCheckResult& Hit,
									AScout* Scout, 
									TArray<FNavMeshPolyBase*>& AddedPolys,
									UBOOL bDiag, 
									INT SubdivisionIteration)
{
	checkSlowish(ParentPoly);
	INT NumAdded = 0;
	FLOAT Step = GetStepSize(SubdivisionIteration);
	TArray<FVector> Positions;

	// top right
	Positions.AddItem(NewNodeTestLocation + FVector(Step*0.5f,Step*0.5f,0.f));
	// bottom right
	Positions.AddItem(NewNodeTestLocation + FVector(-Step*0.5f,Step*0.5f,0.f));
	// bottom left
	Positions.AddItem(NewNodeTestLocation + FVector(-Step*0.5f,-Step*0.5f,0.f));
	// top left
	Positions.AddItem(NewNodeTestLocation + FVector(Step*0.5f,-Step*0.5f,0.f));

	FLOAT LastFailDist = -1.f;
	for(INT Idx=Positions.Num()-1;Idx>=0;Idx--)
	{

		// find the sub-position closest to our previous expansion point, and try to add from there
		INT BestIdx = -1;
		FLOAT BestDistSq = -1.f;
		FLOAT BestDist = -1.;
		for(INT IdxInner=0;IdxInner<Positions.Num();IdxInner++)
		{
			FLOAT ThisDistSq = (Positions(IdxInner) - CurrNodePosWithHeightOffset).SizeSquared();
			if(ThisDistSq < BestDistSq || BestDistSq < 0.f)
			{
				BestIdx = IdxInner;
				BestDistSq = ThisDistSq;
			}
		}



		if(BestIdx >= 0) 
		{
			BestDist = appSqrt(BestDistSq);

			// if the new position is more than 1% further away than the last failed position, don't proceed
			if( LastFailDist == -1.0f || (BestDist/LastFailDist) < NAVMESHGEN_SUBDIVISION_DISTPCT_THRESH)
			{
				// this bool will be TRUE if the spot we're trying to add a node is out of range, or already in the mesh
				UBOOL bInvalidSpot = FALSE;
				FNavMeshPolyBase* NewPoly = ConditionalAddNodeHere(Positions(BestIdx),
					CurrNodePosWithHeightOffset,
					CurrNodePos,
					Hit,
					Scout,
					bInvalidSpot,
					SubdivisionIteration+1,
					bDiag,
					ParentPoly);
				check(ParentPoly != NewPoly || NewPoly == NULL);

				if(NewPoly == NULL && !bInvalidSpot)
				{
					if( NavMeshPtr->Verts.Num() > MAXVERTID )
					{
						return NumAdded;
					}
					else if(SubdivisionIteration+1 < NAVMESHGEN_MAX_SUBDIVISIONS)
					{
						INT SubDivAdd = SubdivideExpandInternal(ParentPoly,Positions(BestIdx),CurrNodePosWithHeightOffset,CurrNodePos,Hit,Scout,AddedPolys,bDiag,SubdivisionIteration+1);
						if(SubDivAdd < 1)
						{
							LastFailDist=BestDist;
						}
						NumAdded += SubDivAdd;

						if( NavMeshPtr->Verts.Num() > MAXVERTID )
						{
							return NumAdded;
						}
					}
					else
					{
						LastFailDist=BestDist;
					}
				}
				else if(NewPoly != NULL)
				{
					//GWorld->GetWorldInfo()->DrawDebugLine(ParentPoly->PolyCenter,NewPoly->PolyCenter,255,255,255,TRUE);
					AddedPolys.AddItem(NewPoly);
					NumAdded++;
				}

				Positions.Remove(BestIdx);
			}
// 			else
// 			{
// 				GWorld->GetWorldInfo()->DrawDebugLine(Positions(BestIdx),CurrNodePos,255,0,0,TRUE);
// 				debugf(TEXT("BLOOP %.2f"),(BestDist/LastFailDist));
// 			}
		}

	}
	return NumAdded;

}

/**
 *	Top level function which will attempt to subdivide an attempted expansion step by halving the check size each time.  This will call SubdivideExpandInternal (which will call itself recursively)
 *  @param ParentPoly - poly we're expending from
 *  @param NewNodeTestLocation - the location we're attempting to expand to
 *  @param CurrNodePosWithHeightOffset - location of predecessor poly with adjusted for height (e.g. pushed up from the ground)
 *  @param CurrNodePos                 - location (on ground) of predecessor poly
 *  @param Hit						   - output hit info storing information about any geometry we encountered
 *  @param Scout					   - ref to scout we are using for parameters
 *  @param bDiag					   - TRUE if this is a diagonal (e.g. non cardinal) expansion from the predecessor poly
 *  @return - ParentPoly if add was succesful 
 */
FNavMeshPolyBase* APylon::SubdivideExpand( FNavMeshPolyBase* ParentPoly, 
										  const FVector& NewNodeTestLocation,
										  const FVector& CurrNodePosWithHeightOffset,
										  const FVector& CurrNodePos,
										  FCheckResult& Hit,
										  AScout* Scout, 
										  UBOOL bDiag)
{

	TArray<FNavMeshPolyBase*> AddedPolys;
	INT Res = SubdivideExpandInternal(ParentPoly,NewNodeTestLocation,CurrNodePosWithHeightOffset,CurrNodePos,Hit,Scout,AddedPolys,bDiag);
	if( NavMeshPtr->Verts.Num() > MAXVERTID )
	{
		return NULL;
	}

	TMap<FNavMeshPolyBase*, UBOOL> MergedPolys;
	if(DO_SUBDIV_MERGE)
	{

		// try and merge added polys together, and with the parent
		for(INT Idx=AddedPolys.Num()-1;Idx>=0;--Idx)
		{
			UBOOL bMergedWithOtherJustAdded=FALSE;

			FNavMeshPolyBase* CurOuterPoly = AddedPolys(Idx);
			check(CurOuterPoly!=ParentPoly);

			// try merging with other polys that just got added first
			for(INT InnerIdx=Idx-1;InnerIdx>=0;--InnerIdx)
			{

				FNavMeshPolyBase* CurInnerPoly = AddedPolys(InnerIdx);
				FNavMeshPolyBase* Merged = NavMeshPtr->TryCombinePolys(CurInnerPoly,CurOuterPoly,MAXVERTID,MAXVERTID,TRUE,FVector(1.f,1.0f,0.f),NULL,FALSE);
				if(Merged != NULL)
				{
					AddedPolys.Remove(Idx);
					AddedPolys.Remove(InnerIdx);
					check(ParentPoly!=Merged);
					AddedPolys.AddItem(Merged);
					MergedPolys.Set(Merged,TRUE);
					Idx=AddedPolys.Num();
					bMergedWithOtherJustAdded=TRUE;
					break;
				}
			}
		}
	}


	AddedPolys.Empty();
	return ParentPoly;
}

/**
 *	used by imported polys to determine if the vertex color is set to a color which indicates the geometry should be 
 *  obstacle mesh
 * @param VertColor - the color the check
 * @return TRUE if the vert should be used for obstacle mesh
 */
UBOOL APylon::IsObstacleColor(FColor& VertColor)
{
	return ( VertColor.R > 200 &&
		VertColor.G < 100 &&
		VertColor.B < 100 );
}

/**
 *	primary workhorse for expansion/exploration.  Will expand in all directions from ParentPoly and attempt to add more polys to the 
 *  mesh
 * @param ParentPoly - poly to expand
 * @param Scout - scout to get params from
 * @Param Diags - out array of diagonal expansion to test later (cardinal directions come first)
 * 
 */
void APylon::ExpandCurrentNode(FNavMeshPolyBase* ParentPoly, AScout* Scout, TArray<FDiagTest>& Diags)
{
	FVector Node = ParentPoly->PolyBuildLoc;
	if(Node.IsNearlyZero())
	{
		Node=ParentPoly->CalcCenter(WORLD_SPACE);
	}

	// don't expand from nodes that are on the interior
	if(!NavMeshPtr->ContainsPointOnBorder(Node))
	{
		return;
	}

	INT AngleIncrement = appTrunc(65536.f / NAVMESHGEN_NUM_ARC_TESTS);

	// find ground from this position
	FCheckResult Hit(1.f);
	static FLOAT NodeHeightOffset = Max<FLOAT>(NAVMESHGEN_ENTITY_HALF_HEIGHT,MaxStepForSlope(2.0f*NAVMESHGEN_STEP_SIZE));
	FVector NodeWithHeightOffset = Node + FVector(0.f,0.f,NodeHeightOffset);
	for(INT AngleIdx=0;AngleIdx<NAVMESHGEN_NUM_ARC_TESTS;AngleIdx++)
	{
		FVector TestDir = FVector(1.f,0.f,0.f).RotateAngleAxis(AngleIncrement*AngleIdx,FVector(0.f,0.f,1.f));

		FVector NewNodeTestLocation = NodeWithHeightOffset + GetExpansionStepSize(TestDir*2.0f*NAVMESHGEN_STEP_SIZE);

		UBOOL bDiagonalExpansion = AngleIdx % 2 != 0;
		UBOOL bAlreadyInMesh=FALSE;
		FNavMeshPolyBase* NewPoly = ConditionalAddNodeHere(NewNodeTestLocation,NodeWithHeightOffset,Node,Hit,Scout,bAlreadyInMesh,0,bDiagonalExpansion, ParentPoly);
		if(NewPoly != NULL)
		{
			WorkingSetPtr->AddTail(NewPoly);
		}
		else if(NAVMESHGEN_MAX_SUBDIVISIONS > 0 && !bAlreadyInMesh)
		{
			if( NavMeshPtr->Verts.Num() > MAXVERTID )
			{
				return;
			}
			else if(!bDiagonalExpansion)
			{
				// try and split expansion into subdivisions and add 
				ParentPoly = SubdivideExpand(ParentPoly,NewNodeTestLocation,NodeWithHeightOffset,Node,Hit,Scout,bDiagonalExpansion);
				if(NavMeshPtr->Verts.Num() > MAXVERTID)
				{
					return;
				}
			}
			else
			{
				Diags.AddItem(FDiagTest(ParentPoly->PolyBuildLoc,NewNodeTestLocation));
			}
		}

	}
}

/**
 *	will attempt to align the location of this pylon to a grid such that all pylons will align with each other and form
 *  nice cosey edges
 * @param Scout - scout to get params from
 * @Loc - location to start from 
 * @return - new grid-snapped location
 */
FVector APylon::SnapSeedLocation( AScout* Scout, FVector& Loc )
{
	// snap start location to stepsize grid to make sure everything is in phase
	// decide which snapped position to take
	FVector SnapPositions[5];

	// center snap
	SnapPositions[0] = Loc.GridSnap(NAVMESHGEN_GRIDSNAPSIZE);

	//** add half step size to location and snap from there so we have other positions to try
	// +X snap
	SnapPositions[1] = (Loc + FVector(NAVMESHGEN_GRIDSNAPSIZE*0.5f,0.f,0.f)).GridSnap(NAVMESHGEN_GRIDSNAPSIZE);
	// -X snap
	SnapPositions[2] = (Loc + FVector(-NAVMESHGEN_GRIDSNAPSIZE*0.5f,0.f,0.f)).GridSnap(NAVMESHGEN_GRIDSNAPSIZE);
	// +Y snap 
	SnapPositions[3] = (Loc + FVector(0.f,NAVMESHGEN_GRIDSNAPSIZE*0.5f,0.f)).GridSnap(NAVMESHGEN_GRIDSNAPSIZE);
	// -Y snap
	SnapPositions[4] = (Loc + FVector(0.f,-NAVMESHGEN_GRIDSNAPSIZE*0.5f,0.f)).GridSnap(NAVMESHGEN_GRIDSNAPSIZE);

	for(INT Idx=0;Idx<5;Idx++)
	{
		SnapPositions[Idx].Z = Loc.Z;// XY phase is what matters!
		FCheckResult Hit(1.0f);
		if(FindGround(SnapPositions[Idx],Hit,Scout))
		{
			return SnapPositions[Idx];
		}
	}

/*
	DrawDebugBox( Loc, FVector(5.f), 255, 0, 0, TRUE );
	DrawDebugLine( Loc, Loc + FVector(0,0,6000), 255, 0, 0, TRUE );
	for( INT i = 0; i < 5; i++ )
	{
		DrawDebugBox( SnapPositions[i], FVector(4.f), 0, 255, 0, TRUE );
		DrawDebugLine( SnapPositions[i], SnapPositions[i] + FVector(0,0,6000), 0, 255, 0, TRUE );
	}
*/

	warnf(TEXT("WARNING! Could not align pylon %s onto grid!  It may be out of phase with the mesh, and fail to link with adjacent pylons."),*GetName());
	return Loc;
}

TDoubleLinkedList<FNavMeshPolyBase*> WorkingSet;
INT NumExpansions = -1;

/**
 *	top level function called to explore from a seed location and create the mesh
 * @param Scout - scout to pull params from
 * @param SeedLocation - location of the seed to start exploration from
 * @return TRUE if successful
 */
UBOOL APylon::Explore_SeedWorkingSet( AScout* Scout, FVector& SeedLocation )
{
	// snap seed to the ground
	FCheckResult Hit(1.f);
	FVector NodeBase = SnapSeedLocation( Scout, SeedLocation );
	if(FindGround(NodeBase,Hit,Scout))
	{
		NodeBase = Hit.Location;
		FNavMeshPolyBase* NewPoly = AddNewNode(NodeBase,Hit.Normal);
		if(NewPoly != NULL)
		{
			WorkingSetPtr->AddTail( NewPoly );
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		debugf(TEXT("Couldn't find ground position for seed %s!"),*GetPathName());
	}

	return (WorkingSetPtr->Num()>0);
}

/**
 *	high level mesh creation function which is responsible for the exploration phase that generates the high
 *  density mesh.  Will walk from seedlocation expanding and adding polys as it verifies their locations
 */
UBOOL APylon::Explore_CreateGraph( AScout* Scout, FVector& SeedLocation )
{
	if( NavMeshPtr == NULL || ObstacleMesh == NULL)
	{
		return FALSE;
	}

	if( !Explore_SeedWorkingSet( Scout, SeedLocation ) )
	{
		debugf(TEXT("ERROR!!! Could not seed expansion for Pylon (%s), check that it is within its expansion bounds, and is not floating, or embedded."),*GetPathName());
		return FALSE;
	}

	// expand from working set nodes until all the nodes in the WS are gone
	FNavMeshPolyBase* CurNode = NULL;
	INT Expansions = 0;
	TArray<FDiagTest> Diags;

	while(WorkingSetPtr->Num() > 0)
	{
		if(NumExpansions > -1 && Expansions++ > NumExpansions)
		{
			break;
		}
		// pop head 
		CurNode = WorkingSetPtr->GetHead()->GetValue();
		WorkingSetPtr->RemoveNode(WorkingSetPtr->GetHead());

		// expand out from this node and add nav points where we deem it possible
		GInitRunaway(); // path objects may end up calling eventFUNC which will eventually cause runaway loop assert if there are a lot of them
		ExpandCurrentNode(CurNode,Scout,Diags);
		if(NavMeshPtr->Verts.Num() > MAXVERTID)
		{
			return FALSE;
		}
	}


	// try and subdivide expand into diagonals 
	for(INT DiagIdx=0;DiagIdx<Diags.Num();DiagIdx++)
	{
		FDiagTest& Diag = Diags(DiagIdx);
		FNavMeshPolyBase* ParentPoly = NavMeshPtr->GetPolyFromPoint(Diag.ParentPos,NAVMESHGEN_MIN_WALKABLE_Z);
		FCheckResult Hit(1.f);
		if(ParentPoly!=NULL)
		{
			ParentPoly=SubdivideExpand(ParentPoly,Diag.Pos,Diag.ParentPos+FVector(0.f,0.f,NAVMESHGEN_ENTITY_HALF_HEIGHT),Diag.ParentPos,Hit,Scout,TRUE);
			if(NavMeshPtr->Verts.Num() > MAXVERTID)
			{
				return FALSE;
			}
		}
	}


	if(ExpansionCullPolys)
	{
		NavMeshPtr->CullSillyPolys();		
	}

	return TRUE;
}

void APylon::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << NavMeshPtr;
	Ar << ObstacleMesh;
	if(DynamicObstacleMesh != NULL)
	{
		Ar << DynamicObstacleMesh;
	}

	// if we're loading a pylon which was before drawscale and drawscale3d were used, reset them to 0 so we don't get unexpected results
	if( Ar.IsSaving() || Ar.IsLoading() )
	{
		if(NavMeshPtr != NULL && NavMeshPtr->NavMeshVersionNum < VER_PYLON_DRAWSCALE)
		{
			DrawScale = 1.0f;
			DrawScale3D = FVector(1.f);
		}
	}
}

void APylon::AddReferencedObjects(TArray<UObject*>& ObjectArray)
{
	Super::AddReferencedObjects(ObjectArray);

	if(NavMeshPtr != NULL)
	{
		AddReferencedObject(ObjectArray, NavMeshPtr);
	}

	if(ObstacleMesh != NULL)
	{
		AddReferencedObject(ObjectArray, ObstacleMesh);
	}

	if(DynamicObstacleMesh != NULL)
	{
		AddReferencedObject(ObjectArray, DynamicObstacleMesh);
	}

}

/**
* Callback used to allow object register its direct object references that are not already covered by
* the token stream.
*
* @param ObjectArray	array to add referenced objects to via AddReferencedObject
* - this is here to keep the navmesh from being GC'd at runtime
*/
void UNavigationMeshBase::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects(ObjectArray);

	// add refs to submeshes 
	for( PolyObstacleInfoList::TIterator It(PolyObstacleInfoMap);It;++It)
	{
		FPolyObstacleInfo& Info = It.Value();
		if(Info.SubMesh != NULL)
		{
			AddReferencedObject(ObjectArray,Info.SubMesh);
		}
	}
}

void APylon::Spawned()
{
	Super::Spawned();

	// Update allowed generators
#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		bAllowRecastGenerator = TRUE;
	}
	else
#endif
	{
		bAllowRecastGenerator = FALSE;
	}
}

void APylon::PostLoad()
{
	Super::PostLoad();

	// Update allowed generators
#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		bAllowRecastGenerator = TRUE;
	}
	else
#endif
	{
		bAllowRecastGenerator = FALSE;
	}

	// Fixup poly ptrs back to the owning nav mesh
	UNavigationMeshBase* NavMesh = GetNavMesh();
	if( NavMesh != NULL )
	{
		for( INT PolyIdx = 0; PolyIdx < NavMesh->Polys.Num(); PolyIdx++ )
		{
			FNavMeshPolyBase& Poly = NavMesh->Polys(PolyIdx);
			Poly.NavMesh = NavMesh;
			Poly.Item = PolyIdx;
			NavMesh->AddPolyToOctree(&Poly);
		}

		NavMeshPtr->StaticVertCount = NavMeshPtr->Verts.Num();

		NavMesh->BuildKDOP();
	}

	if(ObstacleMesh != NULL)
	{
		for( INT PolyIdx = 0; PolyIdx < ObstacleMesh->Polys.Num(); PolyIdx++ )
		{
			FNavMeshPolyBase& Poly = ObstacleMesh->Polys(PolyIdx);
			Poly.NavMesh = ObstacleMesh;
			Poly.Item = PolyIdx;
		}

		if(NavMesh != NULL)
		{
			ObstacleMesh->BuildKDOP();
		}
	}

	// VERIFY containing polys are valid for all verts!
	if(NavMeshPtr != NULL)
	{

		#if(!CONSOLE)
			UBOOL bNeedRebuild=FALSE;
			for(INT VertIdx=0; VertIdx<NavMesh->Verts.Num(); ++VertIdx)
			{
				FMeshVertex& Vert = NavMesh->Verts(VertIdx);

				for (INT ContainingIdx=Vert.PolyIndices.Num()-1; ContainingIdx>=0; --ContainingIdx)
				{
					VERTID TheIdx = Vert.PolyIndices(ContainingIdx);
					if ( !(TheIdx < NavMesh->Polys.Num()))
					{
						bNeedRebuild=TRUE;
						Vert.PolyIndices.Remove(ContainingIdx);
					}
				}
			}

			if(bNeedRebuild)
			{
				warnf(TEXT("WARNING! Map contains invalid pathdata!  Vert to poly indexes are wrong, REBUILD PATHS!"));
			}
		#endif

		
		if(GIsGame)
		{
			// delete storage data, because we don't need it any more 		
			//debugf(TEXT("Emptying storage data for %s.%s  size was: %i "),*GetOutermost()->GetName(),*GetName(),NavMeshPtr->GetAllocatedEdgeStorageDataSize());
			NavMeshPtr->FlushEdgeStorageData();
			
		}
 
		
	}
}


/**
 *	overidden to set pathsrebuild off if the version has gotten out of date
 */
void APylon::PostBeginPlay()
{
	if( bImportedMesh )
	{
		if(NavMeshPtr != NULL)
		{
			NavMeshPtr->InitTransform(this);
		}
		if( ObstacleMesh != NULL )
		{
			ObstacleMesh->InitTransform(this);
		}
	}

	Super::PostBeginPlay();

	if(NavMeshPtr != NULL && NavMeshPtr->VersionAtGenerationTime < VER_MIN_PATHING)
	{
		if(GWorld != NULL && GWorld->GetWorldInfo())
		{
			GWorld->GetWorldInfo()->bPathsRebuilt = FALSE;
		}
	}
}

/**
 * verifies that this pylon is not in conflict with other pylons (e.g. both their start locations are with each other's bounds)
 * @param out_ConflictingPylons - (optional) list of pylons this pylon is in conflict with (optional)
 * @return - TRUE if this pylon is not in conflict
 */
UBOOL APylon::CheckBoundsValidityWithOtherPylons(TArray<APylon*>* out_ConflictingPylons/*=NULL*/)
{
	// MT-> when pylons are first created they're not in the pylon list :( because of this we need the whole darn list
	//for( APylon* CurPylon = GWorld->GetWorldInfo()->PylonList; CurPylon != NULL; CurPylon = CurPylon->NextPylon )
	for ( INT LevelIdx = 0; LevelIdx < GWorld->Levels.Num(); LevelIdx++ )
	{
		ULevel *Level = GWorld->Levels(LevelIdx);
		if (Level != NULL)
		{
			for (INT ActorIdx = 0; ActorIdx < Level->Actors.Num(); ActorIdx++)
			{
				APylon* CurPylon = Cast<APylon>(Level->Actors(ActorIdx));
				if(CurPylon && CurPylon != this &&
					!ImposterPylons.ContainsItem(CurPylon) && 
					IsPtWithinExpansionBounds(CurPylon->Location) &&
					CurPylon->IsPtWithinExpansionBounds(Location))
				{
					if(out_ConflictingPylons != NULL)
					{
						out_ConflictingPylons->AddItem(CurPylon);
					}
					else
					{
						return FALSE;
					}
				}	
			}
		}
	}

	if(out_ConflictingPylons != NULL)
	{
		return out_ConflictingPylons->Num() == 0;
	}
	return TRUE;
}

/**
 *	overidden for custom pylon verification
 */
#if WITH_EDITOR
void APylon::CheckForErrors()
{
	UBOOL bHasValidMesh = NavMeshPtr != NULL && (NavMeshPtr->Polys.Num() > 0 || NavMeshPtr->BuildPolys.Num() > 0);
	if(!bHasValidMesh )
	{
		if( ! IsPtWithinExpansionBounds(Location) )
		{
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString( LocalizeUnrealEd( "MapCheck_Message_PylonOutsideBounds" ) ), TEXT( "PylonOutsideBounds" ) );
		}

		FCheckResult Hit(1.0f);
		AScout* Scout = FPathBuilder::GetScout();
		if( ! FindGround(Location,Hit,Scout) )
		{
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString( LocalizeUnrealEd( "MapCheck_Message_PylonNoGroundPos" ) ), TEXT( "PylonNoGroundPos" ) );
		}
		else
		if(! IsPtWithinExpansionBounds(Hit.Location))
		{
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString( LocalizeUnrealEd( "MapCheck_Message_PylonGroundPosOutsideBounds" ) ), TEXT( "PylonGroundPosOutsideBounds" ) );
		}
		FPathBuilder::DestroyScout();

		if (NavMeshGenerator == NavMeshGenerator_Recast)
		{
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString( LocalizeUnrealEd( "MapCheck_Message_PylonNoGeom" ) ), TEXT("PylonOutsideBounds"));
		}
	}

	if(NavMeshPtr != NULL && NavMeshPtr->VersionAtGenerationTime < VER_MIN_PATHING)
	{
		if(GWorld != NULL && GWorld->GetWorldInfo())
		{
			GWorld->GetWorldInfo()->bPathsRebuilt = FALSE;
		}
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString( LocalizeUnrealEd( "MapCheck_Message_NavMeshOutOfDate" ) ), TEXT( "NavMeshOutOfDate" ) );
	}

	TArray<APylon*> ConflictingPylons;
	CheckBoundsValidityWithOtherPylons(&ConflictingPylons);
	APylon* CurPylon = NULL;

	for( INT Idx=0;Idx<ConflictingPylons.Num();++Idx )
	{
		CurPylon = ConflictingPylons(Idx);
		if(CurPylon != this)
		{
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_PylonsInsideEachOther" ), *GetOutermost()->GetName(), *GetName(), *CurPylon->GetOutermost()->GetName(), *CurPylon->GetName() ) ), TEXT( "PylonsInsideEachOther" ) );
		}	
	}
	Validate();

	Super::CheckForErrors();
}

#endif

void APylon::TogglePathRendering(UBOOL bShouldDrawPaths)
{
	UBOOL bHasComponent = FALSE;
	for (INT Idx = 0; Idx < Components.Num(); Idx++)
	{
		UNavMeshRenderingComponent *PathRenderer = Cast<UNavMeshRenderingComponent>(Components(Idx));
		if (PathRenderer != NULL)
		{
			bHasComponent = TRUE;
			PathRenderer->SetHiddenGame(!bShouldDrawPaths);
			break;
		}
	}
	if (!bHasComponent)
	{
		UNavMeshRenderingComponent *PathRenderer = ConstructObject<UNavMeshRenderingComponent>(UNavMeshRenderingComponent::StaticClass(),this);
		PathRenderer->SetHiddenGame(!bShouldDrawPaths);
		AttachComponent(PathRenderer);
		RenderingComp=PathRenderer;
	}
}

/** Checks to make sure the navigation is at a valid point */
void APylon::Validate()
{
	Super::Validate();

	if( !CheckBoundsValidityWithOtherPylons() )
	{
		// Update sprites by result
		if( GoodSprite )
		{
			GoodSprite->HiddenEditor = TRUE;
		}
		if( BadSprite )
		{
			BadSprite->HiddenEditor = TRUE;
		}
		if( BrokenSprite )
		{
			BrokenSprite->HiddenEditor = FALSE;
		}
	}
	else
	{
		if( BrokenSprite )
		{
			BrokenSprite->HiddenEditor = TRUE;
		}	
	}

	// Force update of sprites
	ForceUpdateComponents( FALSE, FALSE );
}

// UNavigationMeshBase initialize static members 
/**
 *	calls edge constructors for edge data we just loaded
 */
TMap<FName, CtorContainer> UNavigationMeshBase::GEdgeNameCtorMap;
void UNavigationMeshBase::ConstructLoadedEdges()
{
	static UBOOL bEdgeClassesInitialized=FALSE;
	if(!bEdgeClassesInitialized)
	{
		bEdgeClassesInitialized=TRUE;
		InitializeEdgeClasses();

		AScout* GameScout = NULL;
		UClass* ScoutClass = UObject::StaticLoadClass(AScout::StaticClass(), NULL, *GEngine->ScoutClassName, NULL, LOAD_None, NULL);
		if(ScoutClass != NULL && ScoutClass->HasAnyFlags(RF_Native))
		{
			GameScout = ScoutClass->GetDefaultObject<AScout>();
		}
		if( GameScout != NULL )
		{
			GameScout->InitializeCustomEdgeClasses();
		}
		
	}
	

	EdgeDataBuffer.Empty();
	for(INT Idx=0;Idx<EdgeStorageData.Num();Idx++)
	{
		FEdgeStorageDatum& EdgeDatum = EdgeStorageData(Idx);
		CtorContainer* Container = GEdgeNameCtorMap.Find(EdgeDatum.ClassName);
		checkSlowish(Container != NULL);
		FNavMeshEdgeCtor Ctor = Container->Ctor;		
		checkSlowish(Ctor != NULL);

		EdgeDatum.DataPtrOffset = (*Ctor)(EdgeDataBuffer);
	}
	PopulateEdgePtrCache();

}

/**
 *	this will walk through the edge data buffer and set up an array of pointers to the edges for quick access
 */
void UNavigationMeshBase::PopulateEdgePtrCache()
{
	EdgePtrs.Reset();
	CrossPylonEdges.Reset();
	for(INT Idx=0;Idx<EdgeStorageData.Num();Idx++)
	{
		FNavMeshEdgeBase* CurEdge = (FNavMeshEdgeBase*)(&EdgeDataBuffer(0) + EdgeStorageData(Idx).DataPtrOffset);
		CurEdge->Cache(this);
	}
}

/**
 *	performs whatever operations are necessary to get the mesh finalized and ready to be serialized
 */
void UNavigationMeshBase::FixupForSaving()
{
	static TArray<VERTID> VertIndexRemapping;
	VertIndexRemapping.Reset();
	VertIndexRemapping.AddZeroed(Verts.Num());
	
	//Mark all verts used by polys as valid
	for( INT VertIdx = 0; VertIdx < Verts.Num(); VertIdx++ )
	{
		FMeshVertex& Vert = Verts(VertIdx);

		if( Vert.ContainingPolys.Num() <= 0  && Vert.PolyIndices.Num() <= 0)
		{
			VertIndexRemapping(VertIdx) = MAXVERTID;
		}
		else if (Vert.PolyIndices.Num() <= 0)
		{
			Vert.PolyIndices.Reset();
		}
	}


	//mark all verts used by edges as valid
	for (INT EdgeIndex = 0; EdgeIndex < EdgePtrs.Num(); ++EdgeIndex)
	{
		FNavMeshEdgeBase* Edge = EdgePtrs(EdgeIndex);

		VertIndexRemapping(Edge->Vert0) = 0;
		VertIndexRemapping(Edge->Vert1) = 0;
	}

	//Determine index remapping
	INT ValidVertCount = 0;
	for (INT VertIndex = 0; VertIndex < Verts.Num(); ++VertIndex)
	{
		if (VertIndexRemapping(VertIndex) != MAXVERTID)
		{
			VertIndexRemapping(VertIndex) = ValidVertCount;
			ValidVertCount++;
		}
	}

	// Remove all the unused verts
	for( INT VertIdx = Verts.Num()-1; VertIdx >= 0; VertIdx-- )
	{
		if( VertIndexRemapping(VertIdx) == MAXVERTID )
		{
			Verts.Remove( VertIdx );
		}
	}

	//Update edge vert id mappings
	for (INT EdgeIndex = 0; EdgeIndex < EdgePtrs.Num(); ++EdgeIndex)
	{
		FNavMeshEdgeBase* Edge = EdgePtrs(EdgeIndex);
		check(VertIndexRemapping(Edge->Vert0) != MAXVERTID);
		Edge->Vert0 = VertIndexRemapping(Edge->Vert0);

		check(VertIndexRemapping(Edge->Vert1) != MAXVERTID);
		Edge->Vert1 = VertIndexRemapping(Edge->Vert1);
	}

	if( VertHash != NULL )
	{
		// clear out stale vert data from the vert hash
		VertHash->Empty(VertIndexRemapping.Num());
		// re-add persisting verts to the vert hash
		for( INT VertIdx = Verts.Num()-1; VertIdx >= 0; VertIdx-- )
		{
			VertHash->Add(Verts(VertIdx),VertIdx);
		}
	}
	
	// Save poly pointers as objects in array instead of linked list
	for( PolyList::TIterator Itt(BuildPolys.GetHead()); Itt; ++Itt )
	{
		FNavMeshPolyBase* P = *Itt;
		if( P->OctreeId.IsValidId() )
		{
			RemovePolyFromOctree( P );
		}
		for( INT VertIdx = 0; VertIdx < P->PolyVerts.Num(); VertIdx++ )
		{
			check(VertIndexRemapping(P->PolyVerts(VertIdx)) != MAXVERTID);
			P->PolyVerts(VertIdx) = VertIndexRemapping(P->PolyVerts(VertIdx));
		}

		INT PolyIdx = Polys.AddItem( *P );
		FNavMeshPolyBase& Poly = Polys(PolyIdx);
		Poly.Item = PolyIdx;
		P->Item = PolyIdx;
	}

	// after all poly IDs have been set, copy containingpolys into runtime structure
	for(INT VertIdx=0;VertIdx<Verts.Num();++VertIdx)
	{
		FMeshVertex& Vert = Verts(VertIdx);
		
		for( INT ContIdx = 0; ContIdx < Vert.ContainingPolys.Num(); ContIdx++ )
		{
			FNavMeshPolyBase* TestPoly = Vert.ContainingPolys(ContIdx);
			Vert.PolyIndices.AddItem(TestPoly->Item);
		}		
		Vert.ContainingPolys.Empty();
	}

	// cleanup build polys
	for( PolyList::TIterator Itt(BuildPolys.GetHead()); Itt; ++Itt )
	{
		FNavMeshPolyBase* P = *Itt;
		delete P;
	}
	BuildPolys.Clear();

	for( INT PolyIdx = 0; PolyIdx < Polys.Num(); PolyIdx++ )
	{
		FNavMeshPolyBase* Poly = &Polys(PolyIdx);
		if( Poly != NULL && !Poly->OctreeId.IsValidId() )
		{
			AddPolyToOctree( Poly );
		}

		Poly->SetPolyCenter(Poly->CalcCenter(LOCAL_SPACE));
		Poly->PolyNormal = Poly->CalcNormal();
	}

	for( INT VertIdx = 0; VertIdx < Verts.Num(); VertIdx++ )
	{
		FMeshVertex& Vert = Verts(VertIdx);
		Vert.ContainingPolys.Empty();
	}

	StaticVertCount = Verts.Num();
	BuildBounds();
}

/**
 *	this function copies data out of the fast-to-modify build structures and into serializable structures for saving
 */
void UNavigationMeshBase::CopyDataToBuildStructures()
{
	FlushEdges();
	if( Polys.Num() > 0 )
	{
		BuildPolys.Clear();
		for( INT VertIdx = 0; VertIdx < Verts.Num(); ++VertIdx )
		{
			Verts(VertIdx).ContainingPolys.Empty();
		}

		for( INT PolyIdx = 0; PolyIdx < Polys.Num(); ++PolyIdx )
		{
			// Copy into BuildPolys
			FNavMeshPolyBase* CopyPoly;
			if( Polys(PolyIdx).OctreeId.IsValidId() )
			{
				RemovePolyFromOctree(&Polys(PolyIdx));
			}

			if( !Polys(PolyIdx).PolyNormal.IsZero() )
			{
				CopyPoly = AddPolyFromVertIndices(Polys(PolyIdx).PolyVerts,Polys(PolyIdx).PolyHeight);
			}
		}
	}
	Polys.Empty();

	MergePolys(FVector(1.f),FALSE);
	KDOPInitialized=FALSE;
}

/**
* Removes Z of vector if 
* @todo this always projects onto xy plane
*/
FVector ConditionalRemoveZ(const FVector &Point, UBOOL bHoriz)
{
	if ( bHoriz )
	{
		return FVector(Point.X, Point.Y, 0.f); 
	}
	return Point; 
}

// version of pointdisttosegment that outputs T
FLOAT PointDistToSegmentOutT(const FVector &Point, const FVector &StartPoint, const FVector &EndPoint, FVector &OutClosestPoint, FLOAT& OutT) 
{
	const FVector Segment = EndPoint - StartPoint;
	const FVector VectToPoint = Point - StartPoint;

	// See if closest point is before StartPoint
	const FLOAT Dot1 = VectToPoint | Segment;
	if( Dot1 <= 0 )
	{
		OutT = 0.f;
		OutClosestPoint = StartPoint;
		return VectToPoint.Size();
	}

	// See if closest point is beyond EndPoint
	const FLOAT Dot2 = Segment | Segment;
	if( Dot2 <= Dot1 )
	{
		OutT = 1.f;
		OutClosestPoint = EndPoint;
		return (Point - EndPoint).Size();
	}

	// Closest Point is within segment
	OutT = (Dot1 / Dot2);
	OutClosestPoint = StartPoint + Segment * OutT;
	return (Point - OutClosestPoint).Size();
}

// version of pointdisttosegment that outputs T
FLOAT SqPointDistToSegmentOutT(const FVector &Point, const FVector &StartPoint, const FVector &EndPoint, FVector &OutClosestPoint, FLOAT& OutT) 
{
	const FVector Segment = EndPoint - StartPoint;
	const FVector VectToPoint = Point - StartPoint;

	// See if closest point is before StartPoint
	const FLOAT Dot1 = VectToPoint | Segment;
	if( Dot1 <= 0 )
	{
		OutT = 0.f;
		OutClosestPoint = StartPoint;
		return VectToPoint.SizeSquared();
	}

	// See if closest point is beyond EndPoint
	const FLOAT Dot2 = Segment | Segment;
	if( Dot2 <= Dot1 )
	{
		OutT = 1.f;
		OutClosestPoint = EndPoint;
		return (Point - EndPoint).SizeSquared();
	}

	// Closest Point is within segment
	OutT = (Dot1 / Dot2);
	OutClosestPoint = StartPoint + Segment * OutT;
	return (Point - OutClosestPoint).SizeSquared();
}

/**
* Generates vert lists by splitting the passed polygon along the supplied plane
*  @param Poly - poly to split
*  @param Plane - plane to split along
*  @param out_Poly1Verts - output array for verts in poly1
*  @param out_Poly2Verts - output array for vertsin poly2
*  @return TRUE if succesful 
*/

FLOAT SplitSnapSize = 5.f;
#define ISECTPT_SNAP_SIZE SplitSnapSize
UBOOL UNavigationMeshBase::SplitPolyAlongPlane(const FNavMeshPolyBase* Poly, const FPlane& Plane, TArray<FVector>& out_Poly1Verts, TArray<FVector>& out_Poly2Verts)
{
	VERTID NewVertIdx = 0;
	FVector StartPoint(0.f), EndPoint(0.f),IntersectPoint(0.f);

	// Check each edge for an intersection
	INT NumVertsOnPlane = 0;
	for( INT VertIdx = 0; VertIdx < Poly->PolyVerts.Num(); ++VertIdx )
	{
		// Get the endpoints of the edge
		StartPoint = Poly->GetVertLocation(VertIdx,WORLD_SPACE);
		EndPoint = Poly->GetVertLocation( (VertIdx+1) % Poly->PolyVerts.Num(), WORLD_SPACE );

		FLOAT PlaneDot = Plane.PlaneDot(StartPoint);
		UBOOL bStartOnPlane = (Abs<FLOAT>(PlaneDot) < GRIDSIZE);
		UBOOL bEndOnPlane = (Abs<FLOAT>(Plane.PlaneDot(EndPoint)) < GRIDSIZE);
		if(bStartOnPlane)
		{	
			out_Poly1Verts.AddItem(StartPoint);
			out_Poly2Verts.AddItem(StartPoint);
			++NumVertsOnPlane;
		}
		else 
		{
			if(PlaneDot > 0.f)
			{
				out_Poly1Verts.AddItem(StartPoint);
			}
			else
			{
				out_Poly2Verts.AddItem(StartPoint);
			}

			// if this segment intersects, add intersection point to both polys
			if(!bEndOnPlane && SegmentPlaneIntersection(StartPoint, EndPoint, Plane, IntersectPoint) )
			{
				// snap to actual end point if we're close to it
				if(!IntersectPoint.Equals(EndPoint,ISECTPT_SNAP_SIZE))
				{
					out_Poly1Verts.AddItem(IntersectPoint);
					out_Poly2Verts.AddItem(IntersectPoint);					
				}
				else
				{
					// snap: add end point only to poly with start point so it will create whole segment
					// other poly will have it added in next step, since vertex is on its side of split
					if(PlaneDot > 0.f)
					{
						out_Poly1Verts.AddItem(EndPoint);
					}
					else
					{
						out_Poly2Verts.AddItem(EndPoint);
					}
				}
			}
		}
	}

	return (NumVertsOnPlane < 3 && out_Poly1Verts.Num() > 2 && out_Poly2Verts.Num() > 2);
}

/**
 * snap mesh to other mesh's edges.. used for imported meshes to snap/align themselves
 * to edges nearby in the provided mesh
 * @param OtherMesh - mesh to snap to
 */

struct CloseEdge
{
	CloseEdge(const FVector& inStart, const FVector& inEnd, FLOAT inDist)
	{
		Start = inStart;
		End = inEnd;
		Dist = inDist;
	}
	FVector Start;
	FVector End;
	FLOAT Dist;
};
IMPLEMENT_COMPARE_CONSTREF(CloseEdge, ClosestEdgeFirst, { return (A.Dist > B.Dist); })

/**
 * this will attempt to snap verts that are close to another mesh to that mesh (useful for imported meshes)
 * @param OtherMesh - mesh to try and snap verts from me to
 */
FLOAT ExpansionVertSnapDist = 30.f;
#define VERT_SNAP_DIST ExpansionVertSnapDist
void UNavigationMeshBase::SnapMeshVertsToOtherMesh(UNavigationMeshBase* OtherMesh)
{
	UBOOL bMovedAtLeastOneVert = FALSE;
	for(INT VertIdx=0;VertIdx<Verts.Num();++VertIdx)
	{
		FMeshVertex& Vert = Verts(VertIdx);

		if(Vert.ContainingPolys.Num() < 1)
		{
			continue;
		}

		FVector WS_VertLoc = GetVertLocation(VertIdx,WORLD_SPACE);

		// find all polys near the vert in othermesh
		TArray<FNavMeshPolyBase*> NearbyPolys;
		OtherMesh->GetIntersectingPolys(WS_VertLoc,FVector(VERT_SNAP_DIST),NearbyPolys);
		
		// find edge we're closest to, snap to it
		FNavMeshPolyBase* CurPoly = NULL;

		TArray<CloseEdge> CloseEdges;
		for(INT PolyIdx=0;PolyIdx<NearbyPolys.Num();++PolyIdx)
		{
			CurPoly = NearbyPolys(PolyIdx);

			// for each edge of the poly
			for(INT PolyVertIdx=0;PolyVertIdx<CurPoly->PolyVerts.Num();++PolyVertIdx)
			{
				const FVector ThisVertLoc = CurPoly->GetVertLocation(PolyVertIdx,WORLD_SPACE);
				const FVector NextVertLoc = CurPoly->GetVertLocation( (PolyVertIdx+1) % CurPoly->PolyVerts.Num(),WORLD_SPACE);

				FVector ThisSnapPt(0.f);
				const FLOAT ThisDist = PointDistToSegment(WS_VertLoc,ThisVertLoc,NextVertLoc,ThisSnapPt);

				if(ThisDist < VERT_SNAP_DIST)
				{
					CloseEdges.AddItem(CloseEdge(ThisVertLoc,NextVertLoc,ThisDist));
				}
			}
		}

		if(CloseEdges.Num() > 0)
		{

			// we want to snap to edges closest to the point first
			Sort<USE_COMPARE_CONSTREF(CloseEdge,ClosestEdgeFirst)>(&CloseEdges(0),CloseEdges.Num());

			FVector CurrentPos = WS_VertLoc;
			// try to snap to all close edges (or as many as possible)
			for(INT CloseEdgeIdx=0;CloseEdgeIdx<CloseEdges.Num();++CloseEdgeIdx)
			{
				CloseEdge& CE = CloseEdges(CloseEdgeIdx);

				FVector ThisSnapPt(0.f);
				const FLOAT ThisDist = PointDistToSegment(CurrentPos,CE.Start,CE.End,ThisSnapPt);

				if(ThisDist < VERT_SNAP_DIST)
				{
					CurrentPos = ThisSnapPt;
				}
			}

			Vert=W2LTransformFVector(CurrentPos);
			bMovedAtLeastOneVert=true;
		}

		
	}

	// if we moved verts we need to recalc poly centers and normals
	if(bMovedAtLeastOneVert)
	{
		for(PolyList::TIterator Itt(BuildPolys.GetHead());Itt;++Itt)
		{
			FNavMeshPolyBase* Poly = *Itt;
			Poly->RecalcAfterVertChange();
		}
	}
}


/**
*	Subdivides polys at intersections with other meshes to allow for easy cross-pylon edge creation with other NavMeshes
*/
void UNavigationMeshBase::SplitMeshForIntersectingImportedMeshes()
{
	if(ExpansionDoConcaveSlabsOnly)
	{
		return;
	}

	TArray<FNavMeshPolyBase*> PylonIsectPolys;
	TArray<APylon*> OverlappingPylons;
	TArray<FVector> SplitPoly1, SplitPoly2;
	APylon* MyPylon = Cast<APylon>(GetOuter());

	FBox PylonBounds = MyPylon->GetExpansionBounds();
	// Get all Pylons intersecting this NavMesh's Pylon
	UNavigationHandle::GetIntersectingPylons(PylonBounds.GetCenter(), PylonBounds.GetExtent(),OverlappingPylons);

	TArray<APylon*> OverlappingImportedPylons;

	// build a list of imported pylons which overlap this mesh that we need to split for
	for( INT IsectPylonIdx = 0; IsectPylonIdx < OverlappingPylons.Num(); ++IsectPylonIdx )
	{
		if( OverlappingPylons(IsectPylonIdx) != MyPylon && /*!MyPylon->ImposterPylons.ContainsItem(OverlappingPylons(IsectPylonIdx))  && */( OverlappingPylons(IsectPylonIdx)->bImportedMesh ) )
		{
			OverlappingImportedPylons.AddItem(OverlappingPylons(IsectPylonIdx));
		}
	}

	static FLOAT MinArea = NAVMESHGEN_MIN_POLY_AREA;
#define MIN_SPLIT_AREA MinArea
#define MIN_SPLIT_NORMAL_DOT 0.98f	
	// ** FOR EACH imported pylon that intersects our bounds
	//		-> make sure its build structures are populated
	//		-> loop through all polys which intersect between pylons
	//			-> split polys that intersect
	for(INT ImportedIsectIdx = 0; ImportedIsectIdx<OverlappingImportedPylons.Num(); ++ImportedIsectIdx)
	{
		APylon* CurImportedPylon = OverlappingImportedPylons(ImportedIsectIdx);

		CurImportedPylon->NavMeshPtr->SnapMeshVertsToOtherMesh(this);
		CurImportedPylon->ObstacleMesh->SnapMeshVertsToOtherMesh(this);

		TArray<FPlane> SplitPlanesForImportedMesh;

		PolyList::TIterator Itt(BuildPolys.GetHead());
		while( Itt )
		{
			FNavMeshPolyBase* CurrentPoly_ThisPylon = *Itt;
			++Itt;
			if(CurrentPoly_ThisPylon == NULL)
			{
				continue;
			}

			const FVector ThisPolyNormal = CurrentPoly_ThisPylon->GetPolyNormal();

			CurImportedPylon->GetIntersectingPolys(CurrentPoly_ThisPylon->CalcCenter(WORLD_SPACE), CurrentPoly_ThisPylon->GetPolyBounds().GetExtent(), PylonIsectPolys, TRUE);
			// For each potentially intersecting poly
			for( INT IsectPolyIdx = 0; IsectPolyIdx < PylonIsectPolys.Num(); ++IsectPolyIdx )
			{
				FNavMeshPolyBase* CurrentPoly_OtherPylon = PylonIsectPolys(IsectPolyIdx);

				// Check if the polys actually intersect
				if ( CurrentPoly_ThisPylon->IntersectsPoly(CurrentPoly_OtherPylon, WORLD_SPACE, 1.f) ) 
				{
					// Check if the intersecting polys are coplanar
					const FVector OtherPolyNormal = CurrentPoly_OtherPylon->GetPolyNormal();
					
					// if polys are close to the same normal, don't try splitting them
					if (Abs<FLOAT>(ThisPolyNormal | OtherPolyNormal) < MIN_SPLIT_NORMAL_DOT )
					{
						UBOOL bSplitCurPoly = FALSE;
						UBOOL bSplitCurIsectPoly = FALSE;

						FPlane ImportedPoly_Plane = FPlane(CurrentPoly_OtherPylon->GetPolyCenter(WORLD_SPACE),CurrentPoly_OtherPylon->GetPolyNormal(WORLD_SPACE));
						FPlane MyPoly_Plane = FPlane(CurrentPoly_ThisPylon->GetPolyCenter(WORLD_SPACE),CurrentPoly_ThisPylon->GetPolyNormal(WORLD_SPACE));

						// split the poly in this pylon
						bSplitCurPoly = SplitPolyAlongPlane(CurrentPoly_ThisPylon, ImportedPoly_Plane, SplitPoly1, SplitPoly2);

						// Add the new polys, delete the old
						if( bSplitCurPoly )
						{
							FLOAT Area1 = FNavMeshPolyBase::CalcArea(SplitPoly1);
							FLOAT Area2 = FNavMeshPolyBase::CalcArea(SplitPoly2);

							if(Area1 < MIN_SPLIT_AREA || Area2 < MIN_SPLIT_AREA)
							{
								bSplitCurPoly = FALSE;
							}
							else
							{
								RemovePoly(CurrentPoly_ThisPylon);
								CurrentPoly_ThisPylon = NULL;
								AddPoly(SplitPoly1, -1.f, TRUE,-1.f,KINDA_SMALL_NUMBER);
								AddPoly(SplitPoly2, -1.f, TRUE,-1.f,KINDA_SMALL_NUMBER);

								UBOOL bAlreadyInList=FALSE;
								for(INT PlaneIdx=0;PlaneIdx<SplitPlanesForImportedMesh.Num();++PlaneIdx)
								{
									if(SplitPlanesForImportedMesh(PlaneIdx).Equals(MyPoly_Plane,0.01f))
									{
										bAlreadyInList=TRUE;
										break;
									}
								}

								if(!bAlreadyInList)
								{
									SplitPlanesForImportedMesh.AddItem(MyPoly_Plane);
								}															
							}
						}

						SplitPoly1.Empty();
						SplitPoly2.Empty();

						if(bSplitCurPoly)
						{
							Itt = BuildPolys.GetHead();
							break;
						}
					}
				}
			}

			PylonIsectPolys.Empty();
		}

		// *** now split the imported mesh against all splitting planes
		for(INT PlaneIdx=0;PlaneIdx<SplitPlanesForImportedMesh.Num();++PlaneIdx)
		{
			
			const FPlane& CurPlane = SplitPlanesForImportedMesh(PlaneIdx);

			
			PolyList::TIterator Itt(CurImportedPylon->GetNavMesh()->BuildPolys.GetTail());
			INT InfLoopCheck=0;
			INT LoopCheckNum = BuildPolys.Num();
			while( Itt )
			{
				FNavMeshPolyBase* CurrentPoly_ImportedPylon = *Itt;
				--Itt;

				if( InfLoopCheck++ > BuildPolys.Num() )
				{
					warnf(TEXT("Looped too many times in import split of %s!"),*CurImportedPylon->GetName());
					break;
				}

				if(!FPlaneAABBIsect(CurPlane,CurrentPoly_ImportedPylon->GetPolyBounds(WORLD_SPACE)))
				{
					continue;
				}
				
				UBOOL bSplitCurIsectPoly = CurImportedPylon->NavMeshPtr->SplitPolyAlongPlane(CurrentPoly_ImportedPylon, CurPlane, SplitPoly1, SplitPoly2);
				if( bSplitCurIsectPoly )
				{

					FLOAT Area1 = FNavMeshPolyBase::CalcArea(SplitPoly1);
					if(Area1 >= MIN_SPLIT_AREA)
					{
						FNavMeshPolyBase* ResultPoly = CurImportedPylon->NavMeshPtr->AddPoly(SplitPoly1, -1.f, TRUE);
						if( ResultPoly == NULL )
						{
							continue;
						}
					}

					FLOAT Area2 = FNavMeshPolyBase::CalcArea(SplitPoly2);
					if(Area2 >= MIN_SPLIT_AREA)
					{
						FNavMeshPolyBase* ResultPoly = CurImportedPylon->NavMeshPtr->AddPoly(SplitPoly2, -1.f, TRUE);
						if( ResultPoly == NULL )
						{
							continue;
						}
					}

					CurImportedPylon->NavMeshPtr->RemovePoly(CurrentPoly_ImportedPylon);
					CurrentPoly_ImportedPylon = NULL;

				}
				SplitPoly1.Empty();
				SplitPoly2.Empty();
			} // END ForEach poly in imported mesh
		}// END ForEach splitplanes

		CurImportedPylon->NavMeshPtr->SnapMeshVertsToOtherMesh(this);
		CurImportedPylon->ObstacleMesh->SnapMeshVertsToOtherMesh(this);


	}// END ForEach Isecting pylon
}

FORCEINLINE_DEBUGGABLE UBOOL FTriangleAABBOverlapPrecise(const FVector& v0,const FVector& v1,const FVector& v2, const FVector& Ctr, const FVector& Extent)
{
	FSeparatingAxisPointCheck ThePointCheck(v0,v1,v2,Ctr,Extent,100000.f);
	if( ThePointCheck.Hit )
	{
		return TRUE;
	}

	return FALSE;
}

// We are going to avoid culling on equality or even near misses to let the precise intersection figure that out
static VectorRegister VCullSlop = {0.005f,0.005f,0.005f,0.0f}; 

FORCEINLINE_DEBUGGABLE UBOOL FTriangleAABBOverlap(const FVector& v0,const FVector& v1,const FVector& v2, const FVector& Ctr, const FVector& Extent)
{
	const FLOAT* RESTRICT V0Ptr = &v0.X;
	const FLOAT* RESTRICT V1Ptr = &v1.X;
	const FLOAT* RESTRICT V2Ptr = &v2.X;
	const FLOAT* RESTRICT CtrPtr = &Ctr.X;
	const FLOAT* RESTRICT ExtentPtr = &Extent.X; // this is actually the half-extent, ironically, it was computed from a bbox, which we will undo

	VectorRegister VExtent = VectorLoadFloat3_W1(ExtentPtr);  // load a one here so we overlap in W
	VectorRegister V0 = VectorLoadFloat3_W0(V0Ptr);
	VectorRegister V1 = VectorLoadFloat3_W0(V1Ptr);
	VectorRegister V2 = VectorLoadFloat3_W0(V2Ptr);  
	VectorRegister VCtr = VectorLoadFloat3_W0(CtrPtr);
	VExtent = VectorAdd(VExtent,VCullSlop);

	VectorRegister TriBoundMin = VectorMin(V0,V1);
	VectorRegister TriBoundMax = VectorMax(V0,V1);
	VectorRegister BoxMin = VectorSubtract(VCtr,VExtent);
	VectorRegister BoxMax = VectorAdd(VCtr,VExtent);
	TriBoundMin = VectorMin(TriBoundMin,V2);
	TriBoundMax = VectorMax(TriBoundMax,V2);

	DWORD Test1 = VectorAnyGreaterThan(TriBoundMin,BoxMax);
	DWORD Test2 = VectorAnyGreaterThan(BoxMin,TriBoundMax);

	if (Test1 || Test2)  
	{
		return FALSE;
	}
	// failed to cull
	return FTriangleAABBOverlapPrecise(v0,v1,v2,Ctr,Extent);
}

/**
 * returns TRUE if the passed AABB intersects the poly passed in
 * @param Ctr - center of AABB
 * @param Extent - extent of AABB
 * @param CurPoly - poly to check against the AABB
 * @return TRUE if they intersect
 */
UBOOL FPolyAABBIntersectPrecise(const FVector& Ctr, const FVector& Extent, FNavMeshPolyBase* CurPoly)
{
	FMeshVertex* RESTRICT Vert0 = &CurPoly->NavMesh->Verts(CurPoly->PolyVerts(0));
	for(INT VertIdx=CurPoly->PolyVerts.Num()-1;VertIdx>1;VertIdx--)
	{
		VERTID Vert1Idx = CurPoly->PolyVerts(VertIdx);
		VERTID Vert2Idx = CurPoly->PolyVerts(VertIdx-1);
		FMeshVertex* RESTRICT Vert1 = &CurPoly->NavMesh->Verts(Vert1Idx);
		FMeshVertex* RESTRICT Vert2 = &CurPoly->NavMesh->Verts(Vert2Idx);

		if( FTriangleAABBOverlap(*Vert0,*Vert1,*Vert2,Ctr,Extent) )
		{
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
 * returns TRUE if the passed AABB intersects the poly passed in
 * @param Ctr - center of AABB
 * @param Extent - extent of AABB
 * @param CurPoly - poly to check against the AABB
 * @return TRUE if they intersect
 */
UBOOL FPolyAABBIntersect(const FVector& Ctr, const FVector& Extent, FNavMeshPolyBase* CurPoly)
{
	FMeshVertex* RESTRICT Vert0 = &CurPoly->NavMesh->Verts(CurPoly->PolyVerts(0));
	const FLOAT* RESTRICT V0Ptr = &Vert0->X;
	VectorRegister TriBoundMin = VectorLoadFloat3_W0(V0Ptr);
	VectorRegister TriBoundMax = TriBoundMin;

	for(INT VertIdx=CurPoly->PolyVerts.Num() - 1;VertIdx > 0;VertIdx--)
	{
		FMeshVertex* RESTRICT Vert = &CurPoly->NavMesh->Verts(CurPoly->PolyVerts(VertIdx));
		const FLOAT* RESTRICT VPtr = &Vert->X;
		VectorRegister V = VectorLoadFloat3_W0(VPtr);  
		TriBoundMin = VectorMin(TriBoundMin,V);
		TriBoundMax = VectorMax(TriBoundMax,V);
	}

	const FLOAT* RESTRICT CtrPtr = &Ctr.X;
	const FLOAT* RESTRICT ExtentPtr = &Extent.X; // this is actually the half-extent, ironically, it was computed from a bbox, which we will undo
	VectorRegister VExtent = VectorLoadFloat3_W1(ExtentPtr);  // load a one here so we overlap in W
	VectorRegister VCtr = VectorLoadFloat3_W0(CtrPtr);
	VExtent = VectorAdd(VExtent,VCullSlop);
	VectorRegister BoxMin = VectorSubtract(VCtr,VExtent);
	VectorRegister BoxMax = VectorAdd(VCtr,VExtent);
	
	DWORD Test1 = VectorAnyGreaterThan(TriBoundMin,BoxMax);
	DWORD Test2 = VectorAnyGreaterThan(BoxMin,TriBoundMax);

	if (Test1 || Test2)  
	{
		return FALSE;
	}
	return FPolyAABBIntersectPrecise(Ctr,Extent,CurPoly);
}

/**
 * returns TRUE if the passed AABB intersects the poly described by the passed vert locations in CW winding
 * @param Ctr - center of AABB
 * @param Extent - extent of AABB
 * @param PolyLocs - array of vert locations
 * @return TRUE if they intersect
 */
UBOOL FPolyAABBIntersect(const FVector& Ctr, const FVector& Extent, const TArray<FVector>& PolyLocs)
{
	const FVector Vert0 = PolyLocs(0);
	for(INT VertIdx=PolyLocs.Num()-1;VertIdx>1;VertIdx--)
	{
		const FVector Vert1 = PolyLocs(VertIdx);
		const FVector Vert2 = PolyLocs(VertIdx-1);

		if( FTriangleAABBOverlap(Vert0,Vert1,Vert2,Ctr,Extent) )
		{
			return TRUE;
		}
	}

	return FALSE;
}


UBOOL DoesBoxIntersectPolys(const FVector& Ctr,const FVector& Extent, const TArray<FNavMeshPolyBase*>& ObstacleMeshPolys)
{
	for(INT Idx=0;Idx<ObstacleMeshPolys.Num();++Idx)
	{
		FNavMeshPolyBase* CurPoly = ObstacleMeshPolys(Idx);
		// reversed the order here to do the fast test first (and early-out)
		const FLOAT PlaneDot = CurPoly->GetPolyPlane(LOCAL_SPACE).PlaneDot(Ctr);
		if( PlaneDot > 0 && PlaneDot < Extent.GetMax() && 
			FPolyAABBIntersect(Ctr,Extent,CurPoly) )
		{
			return TRUE;
		}
	}

	return FALSE;
}

FORCEINLINE UBOOL ExtentLineCheckMightHitPoly(FNavMeshPolyBase* Poly, const FVector Start, const FVector End, const FVector Extent)
{

	FMeshVertex* RESTRICT Vert0 = &Poly->NavMesh->Verts(Poly->PolyVerts(0));

	UBOOL bHit=FALSE;
	for(INT VertIdx=1;VertIdx<Poly->PolyVerts.Num();++VertIdx)
	{
		VERTID Vert1Idx = Poly->PolyVerts(VertIdx);
		VERTID Vert2Idx = Poly->PolyVerts((VertIdx+1)%Poly->PolyVerts.Num());
		FMeshVertex* RESTRICT Vert1 = &Poly->NavMesh->Verts(Vert1Idx);
		FMeshVertex* RESTRICT Vert2 = &Poly->NavMesh->Verts(Vert2Idx);


		const FLOAT* RESTRICT V0Ptr = &Vert0->X;
		const FLOAT* RESTRICT V1Ptr = &Vert1->X;
		const FLOAT* RESTRICT V2Ptr = &Vert2->X;
		const FLOAT* RESTRICT StartPtr = &Start.X;
		const FLOAT* RESTRICT EndPtr = &End.X;
		const FLOAT* RESTRICT ExtentPtr = &Extent.X; 

		VectorRegister VExtent = VectorLoadFloat3_W1(ExtentPtr);  // load a one here so we overlap in W
		VectorRegister V0 = VectorLoadFloat3_W0(V0Ptr);
		VectorRegister V1 = VectorLoadFloat3_W0(V1Ptr);
		VectorRegister V2 = VectorLoadFloat3_W0(V2Ptr);  
		VectorRegister VStart = VectorLoadFloat3_W0(StartPtr);
		VectorRegister VEnd = VectorLoadFloat3_W0(EndPtr);
		VExtent = VectorAdd(VExtent,VCullSlop);

		VectorRegister VNormal = VectorCross(VectorSubtract(V2,V1),VectorSubtract(V0,V1));
		VectorRegister VRay = VectorSubtract(VStart,VEnd);

		VectorRegister TriBoundMin = VectorMin(V0,V1);
		VectorRegister TriBoundMax = VectorMax(V0,V1);
		VectorRegister BoxMin = VectorSubtract(VectorMin(VStart,VEnd),VExtent);
		VectorRegister BoxMax = VectorAdd(VectorMax(VStart,VEnd),VExtent);
		TriBoundMin = VectorMin(TriBoundMin,V2);
		TriBoundMax = VectorMax(TriBoundMax,V2);

		DWORD Test1 = VectorAnyGreaterThan(TriBoundMin,BoxMax);
		DWORD Test2 = VectorAnyGreaterThan(BoxMin,TriBoundMax);

		if ( !(Test1 || Test2) )
		{
			bHit = TRUE;
		}
	}

	return bHit;
}


/**
 * recursively divides&conquers to find a valid point check location if one exists
 * @param Start - start of segment to find a valid spot on
 * @param End - end of segment to find valid spot on
 * @param Extent - extent to find space for
 * @param ObstacleMeshPolys - list of polys precomputed to be within the area we are looking at that we need to check against
 * @param PylonsToCheckPtr - pointer to array of pylons to pointcheck
 * @param out_StartCheckPos - out param stuffed with valid spot if one is found
 * @param ExistingEdgePairs - list of edge pairs that already exist, useful for re-using start positions and void doing extra work
 * @param out_EdgeGroupID - if we re-used a start point from a larger edge, this will stuffed with that edge's groupID, otherwise it's -1
 * @param MinCheckDist - minimum distance within which we will stop trying to find a start point
 * @return - TRUE if a valid spot is found
 */

UBOOL FindCheckStartPos(const FVector& Start,const FVector& End,const FVector& Extent,const TArray<FNavMeshPolyBase*>& ObstacleMeshPolys, FVector& out_StartCheckPos, const TArray<UNavigationMeshBase::FEdgeWidthPair>* ExistingEdgePairsPtr, INT& out_EdgeGroupID, FLOAT MinCheckDist)
{

	#define MIN_CHECK_START_SIZE_SQ MinCheckDist*MinCheckDist

	FLOAT LastUsedWidth = BIG_NUMBER;
	out_EdgeGroupID=-1;
	if( ExistingEdgePairsPtr != NULL )
	{
		const TArray<UNavigationMeshBase::FEdgeWidthPair>& ExistingEdgePairs=*ExistingEdgePairsPtr;
		// try and re-use start positions from previous edges first
		for(INT EdgePairIdx=0;EdgePairIdx<ExistingEdgePairs.Num();++EdgePairIdx)
		{
			const UNavigationMeshBase::FEdgeWidthPair& CurrPair = ExistingEdgePairs(EdgePairIdx);

			// if it's smaller than the size we've seen before, and bugger than our current size (we want an edge that's one size up from the current)
			if( CurrPair.SupportedWidth > Extent.X )
			{
				if( CurrPair.SupportedWidth < LastUsedWidth )
				{
					LastUsedWidth=CurrPair.SupportedWidth;
				}

				const FVector SegCtr = (CurrPair.Pt0+CurrPair.Pt1)*0.5f;
				FVector Closest(0.f);
				if ( PointDistToSegment(SegCtr,Start,End,Closest) < 1.f )
				{
					out_StartCheckPos = SegCtr;
					out_EdgeGroupID=CurrPair.EdgeGroupID;
					return TRUE;
				}
			}

		}
	}

	// filter polys that don't intersect the edge line
	const FVector MidPt_NoZ = (Start + End)*0.5f;
	FVector MidPt = MidPt_NoZ;
	MidPt.Z += Extent.Z;
	if( !DoesBoxIntersectPolys(MidPt,Extent,ObstacleMeshPolys) )
	{
		out_StartCheckPos = MidPt_NoZ;
		return TRUE;
	}

	// divide and try again!
	if( (Start-MidPt_NoZ).SizeSquared() > MIN_CHECK_START_SIZE_SQ && FindCheckStartPos(Start,MidPt_NoZ,Extent,ObstacleMeshPolys, out_StartCheckPos,NULL,out_EdgeGroupID,MinCheckDist) )
	{
		return TRUE;
	}

	if( (MidPt_NoZ-End).SizeSquared() > MIN_CHECK_START_SIZE_SQ && FindCheckStartPos(MidPt_NoZ,End,Extent,ObstacleMeshPolys, out_StartCheckPos,NULL,out_EdgeGroupID,MinCheckDist) )
	{
		return TRUE;
	}

	return FALSE;
}

UBOOL ExtentLineCheckTriangle(const FVector& v1, const FVector& v2, const FVector& v3, const FVector& Start, const FVector& End, const FVector& Extent, FVector& out_HitLoc, FLOAT& out_HitTime)
{
	FVector HitNormal(0.f);
	FLOAT HitTime=1.0f;

	const FLOAT* RESTRICT V0Ptr = &v1.X;
	const FLOAT* RESTRICT V1Ptr = &v2.X;
	const FLOAT* RESTRICT V2Ptr = &v3.X;
	const FLOAT* RESTRICT StartPtr = &Start.X;
	const FLOAT* RESTRICT EndPtr = &End.X;
	const FLOAT* RESTRICT ExtentPtr = &Extent.X; // this is actually the half-extent, ironically, it was computed from a bbox, which we will undo

	VectorRegister VExtent = VectorLoadFloat3_W1(ExtentPtr);  // load a one here so we overlap in W
	VectorRegister V0 = VectorLoadFloat3_W0(V0Ptr);
	VectorRegister V1 = VectorLoadFloat3_W0(V1Ptr);
	VectorRegister V2 = VectorLoadFloat3_W0(V2Ptr);  
	VectorRegister VStart = VectorLoadFloat3_W0(StartPtr);
	VectorRegister VEnd = VectorLoadFloat3_W0(EndPtr);
	VExtent = VectorAdd(VExtent,VCullSlop);

	VectorRegister VNormal = VectorCross(VectorSubtract(V2,V1),VectorSubtract(V0,V1));
	VectorRegister VRay = VectorSubtract(VStart,VEnd);

	VectorRegister TriBoundMin = VectorMin(V0,V1);
	VectorRegister TriBoundMax = VectorMax(V0,V1);
	VectorRegister VDot = VectorDot3(VNormal,VRay);
	VectorRegister BoxMin = VectorSubtract(VectorMin(VStart,VEnd),VExtent);
	VectorRegister BoxMax = VectorAdd(VectorMax(VStart,VEnd),VExtent);
	TriBoundMin = VectorMin(TriBoundMin,V2);
	TriBoundMax = VectorMax(TriBoundMax,V2);

	DWORD Test1 = VectorAnyGreaterThan(TriBoundMin,BoxMax);
	DWORD Test2 = VectorAnyGreaterThan(BoxMin,TriBoundMax);
	DWORD Test3 = VectorAnyGreaterThan(VectorZero(),VDot);

	if (Test1 || Test2 || Test3)  
	{
		return FALSE;
	}
	// failed to cull
	UBOOL Result = FindSeparatingAxis(v1,v2,v3,Start,End,Extent,HitTime,HitNormal);

	if( Result )
	{
		// make sure the hit is on the right side of the poly (ignore backface hits)
		if ( ((Start-End) | ((v3-v2)^(v1-v2)).SafeNormal()) < KINDA_SMALL_NUMBER )
		{
			Result = !Result;
		}
		else
		{
			// if we hit and this hit is closer than the old one, calculate a hit loc
			if ( HitTime < out_HitTime ) 
			{
				const FVector HitLoc = Start + (End-Start)*HitTime;
				out_HitTime = HitTime;
				out_HitLoc = HitLoc;
			}
		}
	}
 
	return Result;
}

/**
 * performs a linecheck only against the polys in the list
 * @param Start - start of linecheck
 * @param End - .. end of line check
 * @param Extent - extent of linecheck to perform (for swept box check)
 * @param Polys - polys to check against
 * @param out_HitLoc - location on the poly we hit
 * @param out_HitTime - optional param stuffed with hit time
 * @param out_HitPoly - optional param, supplied with the poly that was hit
 * @return TRUE if we hit something
 */
UBOOL UNavigationMeshBase::LineCheckAgainstSpecificPolys(const FVector Start, const FVector End, const FVector Extent, const TArray<FNavMeshPolyBase*>& Polys, FVector& out_HitLoc, FLOAT* out_HitTime/*=NULL*/, FNavMeshPolyBase** out_HitPoly/*=NULL*/)
{

	FLOAT CurrentHitTime = 10.f;
	UBOOL bHit = FALSE;

	FVector outboundHitLoc = out_HitLoc;
	for(INT Idx=0;Idx<Polys.Num();++Idx)
	{
		FNavMeshPolyBase* CurPoly = Polys(Idx);
		check(CurPoly->PolyVerts.Num() > 0);

		FMeshVertex* RESTRICT Vert0 = &CurPoly->NavMesh->Verts(CurPoly->PolyVerts(0));
		for(INT VertIdx=1;VertIdx<CurPoly->PolyVerts.Num();++VertIdx)
		{
			VERTID Vert1Idx = CurPoly->PolyVerts(VertIdx);
			VERTID Vert2Idx = CurPoly->PolyVerts((VertIdx+1)%CurPoly->PolyVerts.Num());
			FMeshVertex* RESTRICT Vert1 = &CurPoly->NavMesh->Verts(Vert1Idx);
			FMeshVertex* RESTRICT Vert2 = &CurPoly->NavMesh->Verts(Vert2Idx);

			
			if( ExtentLineCheckTriangle(*Vert0,*Vert1,*Vert2,Start,End,Extent,outboundHitLoc,CurrentHitTime ) )
			{
				if(out_HitPoly!=NULL)
				{
					*out_HitPoly=CurPoly;
				}
				if(out_HitTime!=NULL)
				{
					*out_HitTime=CurrentHitTime;
				}
				bHit = TRUE;
			}
		}
	}

	out_HitLoc=outboundHitLoc;
	return bHit;

}


/**
 * helper function for BuildEdgesfromSegmentSpan
 * given a starting point will trace out toward end points and find the full supported width for the given size
 * @param ValidStartPos - verified (clear) starting position
 * @param out_SpanStart - the start of the span we're adjusting for the given size (modified to be the proper start position)
 * @param out_SpanEnd - the end of the span we're adjusting for the given size (modified to the proper end position)
 * @param Extent - extent of the box we're getting sizes fore
 * @param ObstacleMeshPolys - list of polys we're testing against
 * @return TRUE if a valid span was found
 */
UBOOL FindEndPointsForSize(const FVector& ValidStartPos, FVector& out_SpanStart, FVector& out_SpanEnd, const FVector& Extent, const TArray<FNavMeshPolyBase*>& ObstacleMeshPolys)
{
	// MT->TODO height offset should be in direction of poly norm to account for walls/vertical meshes
	FVector HeightOffset(0.f);
	HeightOffset.Z = Extent.Z;

	const FVector StartPosWIthZOffset = ValidStartPos + HeightOffset;
	const FVector StartWithZOffset = out_SpanStart + HeightOffset;
	const FVector EndWithZOffset = out_SpanEnd + HeightOffset;

	// span end points will be adjusted to hit locations...
	if( UNavigationMeshBase::LineCheckAgainstSpecificPolys(StartPosWIthZOffset,StartWithZOffset,Extent,ObstacleMeshPolys,out_SpanStart ))
	{
		out_SpanStart -= HeightOffset;
	}

	if ( UNavigationMeshBase::LineCheckAgainstSpecificPolys(StartPosWIthZOffset,EndWithZOffset,Extent,ObstacleMeshPolys,out_SpanEnd ) )
	{
		out_SpanEnd -= HeightOffset;
	}


	FVector Delta = out_SpanStart-out_SpanEnd; 
	return ( (Delta).Size2D() > NAVMESHGEN_MIN_EDGE_LENGTH || Abs<FLOAT>(Delta.Z) > NAVMESHGEN_MAX_STEP_HEIGHT );	
}

#define MIN_SPAN_LENGTH 5.0f
#define MIN_SPAN_LENGTH_SQ 25.0f
UBOOL FindEdgesForSize(const FVector& Start,const FVector& End,const FVector& Extent,const TArray<FNavMeshPolyBase*>& ObstacleMeshPolys, TArray<UNavigationMeshBase::FEdgeWidthPair>& out_EdgePairs, INT& EdgeGroupID, UBOOL bFilter=TRUE, UNavigationMeshBase* NavMesh=NULL, FColor Color = FColor(255,255,255) )
{
	UBOOL bResult = FALSE;
	FVector StartCheckPos(0.f);
	FVector NewEnd=End;
	FVector NewStart=Start;
	INT This_EdgeGroupID=EdgeGroupID;


	static TArray<FNavMeshPolyBase*> Local_ObstacleMeshPolys;
	if(bFilter)
	{
		Local_ObstacleMeshPolys.Reset();
		Local_ObstacleMeshPolys=ObstacleMeshPolys;

		for(INT LocalIdx=Local_ObstacleMeshPolys.Num()-1;LocalIdx>=0;--LocalIdx)
		{
			if ( !ExtentLineCheckMightHitPoly(Local_ObstacleMeshPolys(LocalIdx),Start,End,Extent) )
			{
				Local_ObstacleMeshPolys.RemoveSwap(LocalIdx);
			}
		}
	}


	// determine a sane value to use for limiting the search (minimum size to check)
	FLOAT MinCheckDist = Clamp<FLOAT>((Start-End).Size()/5.0f,5.0f,30.f);

//	GWorld->GetWorldInfo()->DrawDebugLine(NavMesh->L2WTransformFVector(Start+FVector(0,0,10)),NavMesh->L2WTransformFVector(End+FVector(0,0,10)),255,255,255, TRUE);
	
	if( FindCheckStartPos(Start,End,Extent,Local_ObstacleMeshPolys, StartCheckPos, &out_EdgePairs, This_EdgeGroupID, MinCheckDist) )
	{
		// if this is a new group, increment the groupID
		if( This_EdgeGroupID == -1 )
		{
			This_EdgeGroupID = ++EdgeGroupID;
		}

		if( FindEndPointsForSize(StartCheckPos,NewStart,NewEnd,Extent,Local_ObstacleMeshPolys) )
		{
			// then we had a succesful run, and now we need to actually add the edge
			out_EdgePairs.AddItem(UNavigationMeshBase::FEdgeWidthPair(NewStart,NewEnd,Extent.X,This_EdgeGroupID));

//  		GWorld->GetWorldInfo()->DrawDebugLine(NavMesh->L2WTransformFVector(NewStart),NavMesh->L2WTransformFVector(NewEnd),Color.R, Color.G, Color.B, TRUE);
//  		GWorld->GetWorldInfo()->DrawDebugCylinder( NavMesh->L2WTransformFVector(NewStart), NavMesh->L2WTransformFVector(NewStart), Extent.X, 20, Color.R, Color.G, Color.B, TRUE );
//  		GWorld->GetWorldInfo()->DrawDebugCylinder( NavMesh->L2WTransformFVector(NewEnd), NavMesh->L2WTransformFVector(NewEnd), Extent.X, 20, Color.R, Color.G, Color.B, TRUE );
// 			GWorld->GetWorldInfo()->DrawDebugCylinder( NavMesh->L2WTransformFVector(StartCheckPos), NavMesh->L2WTransformFVector(StartCheckPos), Extent.X, 20, Color.R, Color.G, Color.B, TRUE );

			bResult = TRUE;
		}
		else
		{
#if !WITH_RECAST			
			// if endpoints were right on top of each other, push points out so we don't try and check this spot again
			NewStart = StartCheckPos + (Start-StartCheckPos).SafeNormal()*MIN_SPAN_LENGTH;
			NewEnd = StartCheckPos + (End-StartCheckPos).SafeNormal()*MIN_SPAN_LENGTH;
#endif

			
// 			GWorld->GetWorldInfo()->DrawDebugLine(NavMesh->L2WTransformFVector(NewStart),NavMesh->L2WTransformFVector(NewEnd),255, 255, 255, TRUE);
// 			GWorld->GetWorldInfo()->DrawDebugCylinder( NavMesh->L2WTransformFVector(NewStart), NavMesh->L2WTransformFVector(NewStart), Extent.X, 20, 255, 255, 255, TRUE );
// 			GWorld->GetWorldInfo()->DrawDebugCylinder( NavMesh->L2WTransformFVector(NewEnd), NavMesh->L2WTransformFVector(NewEnd), Extent.X, 20, 255, 255, 255, TRUE );
// 			GWorld->GetWorldInfo()->DrawDebugBox( NavMesh->L2WTransformFVector(StartCheckPos), Extent, 255, 255, 255, TRUE );
		}

#if !WITH_RECAST
		// we just added an edge, now try and trace from the found position on all successive sizes and try to add overlapping edges that fit
		// if we haven't yet reached the full extent of the span
		UBOOL bAtStart = (NewStart-Start).SizeSquared() < MIN_SPAN_LENGTH_SQ;
		UBOOL bAtEnd = (NewEnd-End).SizeSquared() < MIN_SPAN_LENGTH_SQ;
	
		if (!bAtStart)
		{
			bResult |= FindEdgesForSize(Start,NewStart,Extent,Local_ObstacleMeshPolys,out_EdgePairs,EdgeGroupID,FALSE,NavMesh,Color);
		}

		if (!bAtEnd)
		{
			bResult |= FindEdgesForSize(NewEnd,End,Extent,Local_ObstacleMeshPolys,out_EdgePairs,EdgeGroupID,FALSE,NavMesh,Color);
		}
#endif
	}

	return bResult;
}

/**
 * will slide along the given span and build a list of edges and supported widths for that span
 * @param Start - start of segment
 * @param End - End of segment
 * @param out_EdgePairs - array to put resulting structs in defining the edges to add
 * @param bSkipDirectOnSurfaces - when TRUE obstacle mesh polys which are diretly above this edge segment will be skipped
                                  (used for path object edges which get built on obstacle mesh)
*/
extern UBOOL TryToLinkPolyToEdge(UNavigationMeshBase* ObstacleMesh, FNavMeshPolyBase& Poly, const FVector& EdgeVert0, const FVector& EdgeVert1, WORD EdgeIdx, UBOOL bDynamicEdge, FNavMeshCrossPylonEdge* CPEdge/*=NULL*/, UBOOL bSkipDirectOnPolys/*=FALSE*/);

void UNavigationMeshBase::BuildEdgesFromSegmentSpan(const FVector& Start, const FVector& End, TArray<FEdgeWidthPair>& out_EdgePairs, UBOOL bSkipDirectOnSurfaces)
{

	AScout* Scout = AScout::GetGameSpecificDefaultScoutObject();
	if( Scout == NULL )
	{
		warnf(TEXT("Could not get scout object for EdgeSpanCreation!!"));
		return;
	}
	

	const FVector Seg_Delta = End-Start;
	const FLOAT Seg_Dist = Seg_Delta.Size();
	const FVector Seg_Dir = Seg_Delta/Seg_Dist;

	// gather a list of obstacle mesh polys we need to check against when finding a starting position
	static TArray<FNavMeshPolyBase*> ObstacleMeshPolys;
	INT LastIdx = Scout->PathSizes.Num() - 1;
	check(LastIdx>=0);
	const FVector LargestExtent = FVector(Scout->PathSizes(LastIdx).Radius,Scout->PathSizes(LastIdx).Radius,Scout->PathSizes(LastIdx).Height); 

	ObstacleMeshPolys.Reset();

	FBox TraceBox = FBox::BuildAABB(Start,LargestExtent);
	TraceBox += FBox::BuildAABB(End,LargestExtent);

	UNavigationHandle::GetAllObstaclePolysFromPos(TraceBox.GetCenter(), TraceBox.GetExtent(),ObstacleMeshPolys);

	for(INT ObstacleMeshPolyIdx=ObstacleMeshPolys.Num()-1;ObstacleMeshPolyIdx>=0;--ObstacleMeshPolyIdx)
	{
		FNavMeshPolyBase* CurObstaclePoly = ObstacleMeshPolys(ObstacleMeshPolyIdx);
		APylon* PylonForPoly = CurObstaclePoly->NavMesh->GetPylon();
		// if this is a poly from the dynamic obstacle mesh, skip it
		if ( PylonForPoly->DynamicObstacleMesh == CurObstaclePoly->NavMesh )
		{
			continue;
		}

		UNavigationMeshBase* ForNavMesh = PylonForPoly->NavMeshPtr;
		// check to see if this obstacle mesh poly should be removed from consideration because it has fully loaded cross pylon edges associated with it (which might not have been filtered in shouldcheckmaterial because the edges are linked to polys with submeshes)
		for(INT EdgeIdx=0;EdgeIdx<CurObstaclePoly->GetNumEdges();++EdgeIdx)
		{
			if( IsEdgeIDToIgnore(CurObstaclePoly->PolyEdges(EdgeIdx)) )
			{
				// if this edge has been marked as one we should be ignoring, remove it from consideration
				ObstacleMeshPolys.RemoveSwap(ObstacleMeshPolyIdx);
				break;
			}
			FNavMeshEdgeBase* Edge = CurObstaclePoly->GetEdgeFromIdx(EdgeIdx,ForNavMesh,TRUE);
			// intentionally not checking IsValid() here, because we are looking for the case where shouldcheckmaterial didn't filter
			// the poly due to the edges being linked to polys with submeshes
			if ( Edge->GetPoly0() != NULL && Edge->GetPoly1() != NULL )
			{
				// this edge is fully loaded, remove the obstacle poly from consideration!
				ObstacleMeshPolys.RemoveSwap(ObstacleMeshPolyIdx);
				break;
			}
		}


		// if we should be skipping obstacle mesh polys which are directly above edge check surfaces, do so
		if( bSkipDirectOnSurfaces )
		{
			if( TryToLinkPolyToEdge(CurObstaclePoly->NavMesh,*CurObstaclePoly,Start,End,MAXWORD,FALSE,NULL,TRUE) )
			{
				// then this obstacle mesh poly is on top of our proposed edge, so skip it
				ObstacleMeshPolys.RemoveSwap(ObstacleMeshPolyIdx);
			}
		}
	}
	// end legwork to get list of obstacle mesh poly to check
	//////////////////////////////////////////////////////////////////////////


	INT EdgeGroupID=0;
	const FVector LS_Start = W2LTransformFVector(Start);
	const FVector LS_End = W2LTransformFVector(End);
	// starting with the largest path size, try and find a valid starting position, and then find the largest contiguous segment 
	for( INT i = Scout->PathSizes.Num()-1; i >= 0; --i)
	{
		const FVector Extent = FVector(Scout->PathSizes(i).Radius,Scout->PathSizes(i).Radius,Scout->PathSizes(i).Height);
		const FColor  C = Scout->EdgePathColors.IsValidIndex(i) ? Scout->EdgePathColors(i) : FColor(128,0,255);
		UBOOL bFoundEdge = FindEdgesForSize(LS_Start,LS_End,Extent,ObstacleMeshPolys,out_EdgePairs,EdgeGroupID,TRUE,this,C);

		//debug
// 		if( !bFoundEdge )
// 		{
// 			GWorld->GetWorldInfo()->DrawDebugLine(Start, End, 255, 255, 255, TRUE );
// 			for( INT p = 0; p < ObstacleMeshPolys.Num(); ++p )
// 			{
// 				ObstacleMeshPolys(p)->DrawPoly( GWorld->PersistentLineBatcher, FColor(255,0,0) );
// 			}
// 			break;
// 		}
	}

	// transform edge points into WS

	for(INT EdgeIdx=0;EdgeIdx<out_EdgePairs.Num();++EdgeIdx)
	{
		FEdgeWidthPair& Pair = out_EdgePairs(EdgeIdx);
		Pair.Pt0 = L2WTransformFVector(Pair.Pt0);
		Pair.Pt1 = L2WTransformFVector(Pair.Pt1);
	}

}
/**
 * BuildEdgesFromSupportedWidths
 * - this will run the length of the edge and split it up by supported width, calling ArbitrateAndAddEdgeForPolys on each segment found
 * @param Vert0 - first vertex of edge
 * @param Vert1 - second vertex of edge
 * @param ConnectedPolys - list of polys we are linking together
 * @param bDynamicEdges - whether we should be added to dynamic lists instead of static ones
 * @param Poly0Obst - navmeshobstacle (if any) assoicated with poly 0
 * @param Poly1Obst - navmeshobstacle (if any) assoicated with poly 1
 * @param OuterEdge - outer test edge we're adding real edges for (used to test vert locs against)
 * @param InnerEdge - inner test edge we're adding real edges for (used to test vert locs against)
 * @param SupportedEdgeWidth - the width of guy this edge supports
 * @return - FALSE if we ran out of vert indexes 
 */
UBOOL UNavigationMeshBase::BuildEdgesFromSupportedWidths( const FVector& Start,
									const FVector& End,
									TArray<FNavMeshPolyBase*>& ConnectedPolys,
									UBOOL bAddDynamicEdges,
									IInterface_NavMeshPathObstacle* Poly0Obstacle,
									IInterface_NavMeshPathObstacle* Poly1Obstacle,
									FNavMeshEdgeBase* OuterEdge,
									FNavMeshEdgeBase* InnerEdge,
									PolyToPOMap* POMap/*=NULL*/
									)
{


	static TArray<FEdgeWidthPair> Edges;
	Edges.Reset();

	IInterface_NavMeshPathObject* Poly0PO = NULL;
	IInterface_NavMeshPathObject* Poly1PO = NULL;

	if( POMap != NULL )
	{
		Poly0PO = POMap->FindRef(ConnectedPolys(0));
		Poly1PO = POMap->FindRef(ConnectedPolys(1));
	}

	BuildEdgesFromSegmentSpan(Start,End,Edges,(Poly0PO != NULL || Poly1PO != NULL));
	
	for( INT Idx=0;Idx<Edges.Num();++Idx)
	{

		FEdgeWidthPair& CurEdge = Edges(Idx);

		if( !ArbitrateAndAddEdgeForPolys(CurEdge.Pt0,CurEdge.Pt1,
									ConnectedPolys,bAddDynamicEdges,
									Poly0Obstacle,Poly1Obstacle,
									OuterEdge,InnerEdge,
									CurEdge.SupportedWidth,
									CurEdge.EdgeGroupID,
									Poly0PO,Poly1PO))
		{
			return FALSE;
		}
	}

	// we only return FALSE if we hit max vert count
	return TRUE;
}


/**
* ArbitrateAndAddEdgeForPolys
* - this is called from CreateEdgeConnectionsInternal and will determine what edges need to be added
*   for the polys passed.. including path obstacle edge arbitration
* @param Vert1 - first vertex of edge
* @param Vert2 - second vertex of edge
* @param ConnectedPolys - list of polys we are linking together
* @param bDynamicEdges - whether we should be added to dynamic lists instead of static ones
* @param Poly0Obst - navmeshobstacle (if any) assoicated with poly 0
* @param Poly1Obst - navmeshobstacle (if any) assoicated with poly 1
* @param SupporteEdgeWidth - the width of guy this edge supports
* @param Poly0PO - path object associated with poly 0 
* @param Poly1PO - path object associated with poly 1
* @return - FALSE if we ran out of vert indexes 
*/
UBOOL UNavigationMeshBase::ArbitrateAndAddEdgeForPolys( const FVector& Vert0,
										  const FVector& Vert1,
										  const TArray<FNavMeshPolyBase*>& InConnectedPolys,
										  UBOOL bAddDynamicEdges,
										  IInterface_NavMeshPathObstacle* Poly0Obstacle,
										  IInterface_NavMeshPathObstacle* Poly1Obstacle,
										  FNavMeshEdgeBase* OuterEdge,
										  FNavMeshEdgeBase* InnerEdge, 
										  FLOAT SupportedEdgeWidth,
										  BYTE EdgeGroupID,
										  IInterface_NavMeshPathObject* Poly0PO/* = NULL*/,
										  IInterface_NavMeshPathObject* Poly1PO/* = NULL*/
										  )
{
	static TArray<FNavMeshPolyBase*> Local_ConnectedPolys;
	Local_ConnectedPolys=InConnectedPolys;
	EEdgeHandlingStatus Status = EHS_AddedNone;

	// if there are obstacles which have internal geometry attached to either poly
	// and they're different for each poly give that obstacle a chacne to dictate the type of edge created
	if( (Poly0Obstacle != NULL||Poly1Obstacle!=NULL) && Poly0Obstacle != Poly1Obstacle )
	{
		// we need ot call this for both obstacles if they're non-null because each obstacle might want a different edge leading into it
		// (e.g. need two edges one in each direction linked to corresponding obstacle)		
		// first obstacle gets first pick
		// MT-Note: Poly0 (ConnectedPolys0) being passed into addobstacle edge should always be associated with the 
		//			path object we're calling addobstacleedge for! so swap stuff if we need to 
		if(Poly0Obstacle != NULL)
		{
			Status = Poly0Obstacle->AddObstacleEdge(Status,Vert0,Vert1,Local_ConnectedPolys,bAddDynamicEdges,0,SupportedEdgeWidth,EdgeGroupID);
		}
		if(Poly1Obstacle != NULL)
		{
			Status = Poly1Obstacle->AddObstacleEdge(Status,Vert0,Vert1,Local_ConnectedPolys,bAddDynamicEdges,1,SupportedEdgeWidth,EdgeGroupID);
		}
	}
	else if( Poly1PO != Poly0PO )
	{
		if( Poly0PO != NULL )
		{
			Status = Poly0PO->AddStaticEdgeIntoThisPO(Status,Vert0,Vert1,Local_ConnectedPolys,0,SupportedEdgeWidth,EdgeGroupID);
		}

		if( Poly1PO != NULL )
		{
			Status = Poly1PO->AddStaticEdgeIntoThisPO(Status,Vert0,Vert1,Local_ConnectedPolys,1,SupportedEdgeWidth,EdgeGroupID);
		}
	}



	if( Status == EHS_Added0to1 )
	{
		// then we need to reverse the order of the edges so we get an edge in the opposite dir
		Local_ConnectedPolys.SwapItems(0,1);
		FNavMeshEdgeBase* Swap = OuterEdge;
		OuterEdge = InnerEdge;
		InnerEdge = Swap;
	}

	VERTID Poly0Vert0ID = MAXVERTID, Poly0Vert1ID = MAXVERTID, Poly1Vert0ID = MAXVERTID, Poly1Vert1ID = MAXVERTID;

	// if we're going to be adding edges try and find vertIDs to re-use first so we don't have to call FindVert
	if( Status != EHS_AddedBothDirs )
	{
		UBOOL bZeroLengthEdge = ((Vert0-Vert1).SizeSquared() < GRIDSIZE);
		const FVector OuterVert0Loc = OuterEdge->GetVertLocation(0);
		const FVector OuterVert1Loc = OuterEdge->GetVertLocation(1);

		const FVector InnerVert0Loc = InnerEdge->GetVertLocation(0);
		const FVector InnerVert1Loc = InnerEdge->GetVertLocation(1);

		// Vert0, Poly0
		if( (OuterVert0Loc-Vert0).SizeSquared() <= GRIDSIZE )
		{
			Poly0Vert0ID = OuterEdge->Vert0;
		}
		if( Poly0Vert0ID == MAXVERTID && (OuterVert1Loc-Vert0).SizeSquared() <= GRIDSIZE)
		{
			Poly0Vert0ID = OuterEdge->Vert1;
		}

		// Vert1, Poly0
		if( bZeroLengthEdge )
		{
			Poly0Vert1ID = Poly0Vert0ID;
		}
		else
		{
			if( (OuterVert0Loc-Vert1).SizeSquared() <= GRIDSIZE )
			{
				Poly0Vert1ID = OuterEdge->Vert0;
			}
			if( Poly0Vert1ID == MAXVERTID && (OuterVert1Loc-Vert1).SizeSquared() <= GRIDSIZE)
			{
				Poly0Vert1ID = OuterEdge->Vert1;
			}
		}

		// Vert0, Poly1
		if( (InnerVert0Loc-Vert0).SizeSquared() <= GRIDSIZE )
		{
			Poly1Vert0ID = InnerEdge->Vert0;
		}
		if( Poly1Vert0ID == MAXVERTID && (InnerVert1Loc-Vert0).SizeSquared() <= GRIDSIZE)
		{
			Poly1Vert0ID = InnerEdge->Vert1;
		}

		// Vert1, Poly1
		if( bZeroLengthEdge )
		{
			Poly1Vert1ID = Poly1Vert0ID;
		}
		else
		{
			if( (InnerVert0Loc-Vert1).SizeSquared() <= GRIDSIZE )
			{
				Poly1Vert1ID = InnerEdge->Vert0;
			}
			if( Poly1Vert1ID == MAXVERTID && (InnerVert0Loc-Vert1).SizeSquared() <= GRIDSIZE)
			{
				Poly1Vert1ID = InnerEdge->Vert1;
			}
		}
	}

	switch(Status)
	{
	case EHS_AddedBothDirs:
		// we done!
		return TRUE;

	case EHS_Added0to1:
	case EHS_Added1to0:
		if(bAddDynamicEdges)
		{
			AddDynamicCrossPylonEdge<FNavMeshCrossPylonEdge>(Vert0, Vert1, Local_ConnectedPolys,SupportedEdgeWidth,EdgeGroupID,TRUE,NULL,Poly0Vert0ID,Poly0Vert1ID,Poly1Vert0ID,Poly1Vert1ID);
		}
		else if( !AddEdge<FNavMeshBasicOneWayEdge>( Vert0, Vert1, SupportedEdgeWidth,EdgeGroupID, &Local_ConnectedPolys,NULL,TRUE ) )
		{
			return FALSE;
		}
		break;

	case EHS_AddedNone:
		// then add an edge in both dir
		if(bAddDynamicEdges)
		{
			AddDynamicCrossPylonEdge<FNavMeshCrossPylonEdge>(Vert0, Vert1, Local_ConnectedPolys, SupportedEdgeWidth,EdgeGroupID, FALSE,NULL,Poly0Vert0ID,Poly0Vert1ID,Poly1Vert0ID,Poly1Vert1ID);
		}
		else if( !AddEdge<FNavMeshEdgeBase>( Vert0, Vert1, SupportedEdgeWidth,EdgeGroupID , &Local_ConnectedPolys) )
		{
			return FALSE;
		}
		break;
	}

	return TRUE;

}


/**
 * CreateEdgeConnectionsInternal
 * -internal function called by CreateEdgeConnections which does the heavy lifting for edge creation
 * @param Edges - list of polygon line segments to use for creation of saved/real Edge Connections
 * @param bTest - whether this is a test only 
 * @param bCrossPylonEdgesOnly - if TRUE only cross pylon edges will be created 
 * @param out_Edges - optional output array of found edges (when supplied actual edges will not be created, instead this array is populated)
 * @param EdgeDetlaOverride - allows override of MAX_EDGE_DELTA 
 * @param MinEdgeLengthOverride - allows override of MIN_EDGE_LENGTH
 * @param POMap - optional param for a map that links polys to path objects (used offline to link edges to POs)
 */
#define MIN_EDGE_LENGTH NAVMESHGEN_MIN_EDGE_LENGTH

FLOAT ExpansionEdgeVertTolerance = 1.5f;
#define EDGE_VERT_TOLERANCE ExpansionEdgeVertTolerance
#define EDGE_VERT_TOLERANCE_SQ 2.25f

FLOAT DistFromEdgeForFacingPolyCheck = 150.f;
#define DIST_FROM_EDGE_FOR_FACING_CHECK DistFromEdgeForFacingPolyCheck
void UNavigationMeshBase::CreateEdgeConnectionsInternal( TArray<FNavMeshEdgeBase>& Edges,
														UBOOL bTest/* = FALSE*/,
														UBOOL bCrossPylonEdgesOnly/* = FALSE*/,
														UBOOL bAddDynamicEdges/* = FALSE*/,
														TArray<FEdgeTuple>* out_Edges/* = NULL*/,
														FLOAT EdgeDeltaOverride/*=-1.f*/,
														FLOAT MinEdgeLengthOverride/*=-1.f*/,
														PolyToPOMap* POMap/*=NULL*/)
{
	//SCOPE_QUICK_TIMERX(CreateEdgeConnectsIntern)
	
	INT Cnt = 0;
	UBOOL bDone = FALSE;
	APylon* Pylon = Cast<APylon>(GetOuter());

	FLOAT MinEdgeLength = (MinEdgeLengthOverride>-1.f) ? MinEdgeLengthOverride : MIN_EDGE_LENGTH;

	// Loop through edges
	// Check if smaller edge is along the big edge and divide up the edge
	for( INT OuterIdx = 0; OuterIdx < Edges.Num(); OuterIdx++ )
	{
		FNavMeshEdgeBase& OuterEdge = Edges(OuterIdx);

		if(OuterEdge.NavMesh != this)
		{
			continue;
		}


		FVector OuterEdgeVert0 = OuterEdge.GetVertLocation(0);
		FVector OuterEdgeVert1 = OuterEdge.GetVertLocation(1);

		FVector OuterEdgeVert0NoZ = ConditionalRemoveZ(OuterEdgeVert0, !Pylon->bImportedMesh);
		FVector OuterEdgeVert1NoZ = ConditionalRemoveZ(OuterEdgeVert1, !Pylon->bImportedMesh);


		// list of edges added for this segment (PRE sorted based on distance to poly0 in segment)
		TDoubleLinkedList<WORD> AddedEdges;

		for( INT InnerIdx = 0; InnerIdx < Edges.Num(); InnerIdx++ )
		{
			FNavMeshEdgeBase& InnerEdge = Edges(InnerIdx);

			// if we're checking cross-pylon edges and these edges are in the same mesh, skipo it!
			if(bCrossPylonEdgesOnly && InnerEdge.NavMesh == OuterEdge.NavMesh)
			{
				continue;
			}

			// Skip if it's the same edge
			if( InnerIdx == OuterIdx )
			{
				continue;
			}

			FNavMeshPolyBase* PolyInner = InnerEdge.BuildTempEdgePolys(0);
			FNavMeshPolyBase* PolyOuter = OuterEdge.BuildTempEdgePolys(0);
			// if this is somehow an edge from the same poly, skip it
			if(PolyInner == PolyOuter)
			{
				continue;
			}

			// if the polys are already connected, bail
			
			if( PolyInner->GetEdgeTo(PolyOuter) && PolyOuter->GetEdgeTo(PolyInner) )
			{
				continue;
			}
			
			// see if the two edges are near each other
			FVector InnerEdgeVert0 = InnerEdge.GetVertLocation(0);
			FVector InnerEdgeVert1= InnerEdge.GetVertLocation(1);

			FVector InnerEdgeVert0NoZ = ConditionalRemoveZ(InnerEdgeVert0, !Pylon->bImportedMesh);
			FVector InnerEdgeVert1NoZ = ConditionalRemoveZ(InnerEdgeVert1, !Pylon->bImportedMesh);
			
			FVector ClosestPtOnOuterEdge(0.f), ClosestPtOnInnerEdge(0.f);
			SegmentDistToSegmentSafe(OuterEdgeVert0NoZ,OuterEdgeVert1NoZ,InnerEdgeVert0NoZ,InnerEdgeVert1NoZ,ClosestPtOnOuterEdge,ClosestPtOnInnerEdge);

			const FLOAT ClosestPtInnerOuterDistSq = (ClosestPtOnInnerEdge-ClosestPtOnOuterEdge).SizeSquared();
			if( ClosestPtInnerOuterDistSq > EDGE_VERT_TOLERANCE * EDGE_VERT_TOLERANCE)
			{
				continue;
			}


			// find closest points on outer edge to inner edge verts 
			// Vert 0 of inner edge, mapped to outer edge
			FLOAT T_vert0=0.f;
			FVector InnerVert0ToOuterEdge(0.f);
			FLOAT Vert0_Dist = PointDistToSegmentOutT(InnerEdgeVert0NoZ,OuterEdgeVert0NoZ,OuterEdgeVert1NoZ,InnerVert0ToOuterEdge,T_vert0);
			// Vert 1 of inner edge, mapped to outer edge
			FLOAT T_vert1=0.f;
			FVector InnerVert1ToOuterEdge(0.f);
			FLOAT Vert1_Dist = PointDistToSegmentOutT(InnerEdgeVert1NoZ,OuterEdgeVert0NoZ,OuterEdgeVert1NoZ,InnerVert1ToOuterEdge,T_vert1);

			const FLOAT InnerVertMappedToOuterDistSq = (InnerVert0ToOuterEdge-InnerVert1ToOuterEdge).SizeSquared();
			if( InnerVertMappedToOuterDistSq < MinEdgeLength*MinEdgeLength)
			{
				continue;
			}

					
			// now map the closest points on outer back to inner edge so we can check height deltas
			FLOAT T_renap_vert0=0.f;
			FVector ClosestPtOnInnerEdgeToMappedPoint0(0.f);
			PointDistToSegmentOutT(InnerVert0ToOuterEdge,InnerEdgeVert0NoZ,InnerEdgeVert1NoZ,ClosestPtOnInnerEdgeToMappedPoint0,T_renap_vert0);

			FLOAT T_renap_vert1=0.f;
			FVector ClosestPtOnInnerEdgeToMappedPoint1(0.f);
			PointDistToSegmentOutT(InnerVert1ToOuterEdge,InnerEdgeVert0NoZ,InnerEdgeVert1NoZ,ClosestPtOnInnerEdgeToMappedPoint1,T_renap_vert1);


			// if this is not an imported mesh, restore Z value to closest points found
			if( !Pylon->bImportedMesh )
			{
				FVector OuterDelta = (OuterEdgeVert1 - OuterEdgeVert0);
				InnerVert0ToOuterEdge = OuterEdgeVert0 + (OuterDelta * T_vert0);
				InnerVert1ToOuterEdge = OuterEdgeVert0 + (OuterDelta * T_vert1);

				FVector InnerDelta = (InnerEdgeVert1 - InnerEdgeVert0);
				ClosestPtOnInnerEdgeToMappedPoint0 = InnerEdgeVert0 + (InnerDelta * T_renap_vert0);
				ClosestPtOnInnerEdgeToMappedPoint1 = InnerEdgeVert0 + (InnerDelta * T_renap_vert1);
			}

			// make sure that these polys are eligible to be connected
			static UBOOL bDoAwayFacingCheck = TRUE;
			if(bDoAwayFacingCheck && Pylon->bImportedMesh)
			{
				//  -- don't connect polys which are facing away from each other (e.g. a wall poly facing the opposite direction of a ground poly)
				FNavMeshPolyBase* Poly1 = InnerEdge.BuildTempEdgePolys(0);
				FNavMeshPolyBase* Poly2 = OuterEdge.BuildTempEdgePolys(0);

				FPlane Poly1Plane = Poly1->GetPolyPlane();
				FPlane Poly2Plane = Poly2->GetPolyPlane();

				FVector Poly1_Ctr = Poly1->GetPolyCenter();
				FVector Poly2_Ctr = Poly2->GetPolyCenter();
				FVector EdgeDir = (InnerVert0ToOuterEdge - InnerVert1ToOuterEdge);

				/// Poly1
				// find a point a fixed distance away from the edge which is on the plane of the poly
				FVector Dir = (Poly1Plane ^ EdgeDir).SafeNormal();
				// need a direction which moves away perp to the edge and toward the poly, so if we're moving opposite direction invert
				FVector DeltaToCtr = (Poly1_Ctr-InnerVert0ToOuterEdge);
				if ((Dir | DeltaToCtr) < (-Dir | DeltaToCtr))
				{
					Dir *= -1.0f;
				}			
				FVector Poly1TestPt = InnerVert0ToOuterEdge + Dir*DIST_FROM_EDGE_FOR_FACING_CHECK;
				/// Poly2
				// find a point a fixed distance away from the edge which is on the plane of the poly
				Dir = (Poly2Plane ^ EdgeDir).SafeNormal();
				// need a direction which moves away perp to the edge and toward the poly, so if we're moving opposite direction invert
				DeltaToCtr = (Poly2_Ctr-InnerVert0ToOuterEdge);
				if ((Dir | DeltaToCtr) < (-Dir | DeltaToCtr))
				{
					Dir *= -1.0f;
				}			
				FVector Poly2TestPt = InnerVert0ToOuterEdge + Dir*DIST_FROM_EDGE_FOR_FACING_CHECK;

				FLOAT Poly2PlaneDot = Poly2Plane.PlaneDot(Poly1TestPt);
				FLOAT Poly1PlaneDot = Poly1Plane.PlaneDot(Poly2TestPt);

				FLOAT Poly1Ctr_to_Poly2_Sign = ( Poly2PlaneDot > -NAVMESHGEN_STEP_SIZE) ? 1.f : -1.f;
				FLOAT Poly2Ctr_to_Poly1_Sign = ( Poly1PlaneDot > -NAVMESHGEN_STEP_SIZE) ? 1.f : -1.f;
				if( Poly1Ctr_to_Poly2_Sign != Poly2Ctr_to_Poly1_Sign )
				{
					continue;
				}
			}

			// if the mapped points on outer edge are the same point, then this isn't a valid candidate
			if((InnerVert0ToOuterEdge-InnerVert1ToOuterEdge).SizeSquared() < MinEdgeLength*MinEdgeLength)
			{
				continue;
			}


			FVector Delta0 = ClosestPtOnInnerEdgeToMappedPoint0 - InnerVert0ToOuterEdge;
			FVector Delta1 = ClosestPtOnInnerEdgeToMappedPoint1 - InnerVert1ToOuterEdge;

			if(!Pylon->bImportedMesh)
			{
				Delta0.Z = 0.f;
				Delta1.Z = 0.f;
			}

			FLOAT MaxEdgeDelta = EdgeDeltaOverride;
			if(MaxEdgeDelta < 0.f)
			{
				MaxEdgeDelta=NAVMESHGEN_MAX_EDGE_DELTA;
			}
			if( Delta0.SizeSquared() > MaxEdgeDelta*MaxEdgeDelta||
				Delta1.SizeSquared() > MaxEdgeDelta*MaxEdgeDelta)
			{
				continue;
			}

			FLOAT Vert0ZDelta = Abs<FLOAT>(ClosestPtOnInnerEdgeToMappedPoint0.Z - InnerVert0ToOuterEdge.Z);
			FLOAT Vert1ZDelta = Abs<FLOAT>(ClosestPtOnInnerEdgeToMappedPoint1.Z - InnerVert1ToOuterEdge.Z);

			if( Vert0ZDelta < NAVMESHGEN_MAX_STEP_HEIGHT && Vert1ZDelta < NAVMESHGEN_MAX_STEP_HEIGHT )
			{
				TArray<FNavMeshPolyBase*> ConnectedPolys;
				ConnectedPolys.Append(OuterEdge.BuildTempEdgePolys);
				ConnectedPolys.Append(InnerEdge.BuildTempEdgePolys);


				if(!InnerVert0ToOuterEdge.Equals(InnerVert1ToOuterEdge,GRIDSIZE))
				{
					if(out_Edges != NULL)
					{
						out_Edges->AddItem(FEdgeTuple(InnerVert0ToOuterEdge,
													  InnerVert1ToOuterEdge,
													  ConnectedPolys(0),
													  OuterEdge.Vert0,
													  OuterEdge.Vert1,
													  ConnectedPolys(1),
													  InnerEdge.Vert0,
													  InnerEdge.Vert1,
													  &OuterEdge,
													  &InnerEdge));
					}
					else if( !BuildEdgesFromSupportedWidths(InnerVert0ToOuterEdge,
															InnerVert1ToOuterEdge,
															ConnectedPolys,
															bAddDynamicEdges,
															NULL,
															NULL,
															&OuterEdge,
															&InnerEdge,
															POMap))
					{
						return;
					}
				}
			}
		}
	}
}

/**
 * Looks for an edge from poly1 to poly2, does no edge derefs to avoid map hits where possible
 * @param Poly1 - first poly
 * @param Poly2 - second poly 
 * @return TRUE if polys are connected
 */
UBOOL ExistsEdgeFromPoly1ToPoly2Fast(FNavMeshPolyBase* Poly1, FNavMeshPolyBase* Poly2)
{
	UNavigationMeshBase* NavMesh1 = Poly1->NavMesh;
	UNavigationMeshBase* NavMesh2 = Poly2->NavMesh;

	UBOOL bNeedsCrossPylon = NavMesh1 != NavMesh2;

	INT NumEdges = Poly1->GetNumEdges();
	for(INT EdgeIdx=0;EdgeIdx<NumEdges;EdgeIdx++)
	{
		FNavMeshEdgeBase* Edge = Poly1->GetEdgeFromIdx(EdgeIdx,NULL,TRUE);
		if( Edge != NULL )
		{
			if( Edge->IsCrossPylon() )
			{
				FNavMeshCrossPylonEdge* CPEdge = static_cast<FNavMeshCrossPylonEdge*>(Edge);
				
				if( ( CPEdge->Poly0Ref==Poly1 && CPEdge->Poly1Ref==Poly2 ) ||
					( CPEdge->Poly0Ref==Poly2 && CPEdge->Poly1Ref==Poly1 ) )
				{
					return TRUE;
				}
			}
			else if( !bNeedsCrossPylon )
			{
				if ( (Poly1->Item == Edge->Poly0 && Poly2->Item == Edge->Poly1) ||
						 (Poly2->Item == Edge->Poly0 && Poly1->Item == Edge->Poly1) )
				{
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

/**
 * returns TRUE if the two passed polys have edges interconnecting them
 */
UBOOL ArePolysConnected(FNavMeshPolyBase* Poly1, FNavMeshPolyBase* Poly2)
{
	// get edge from poly1 to poly 2

	if( ExistsEdgeFromPoly1ToPoly2Fast(Poly1, Poly2) && ExistsEdgeFromPoly1ToPoly2Fast(Poly2, Poly1) )
	{
		return TRUE;
	}

	return FALSE;
}
/**
 * CreateEdgeConnectionsInternalFast
 * -internal function called by CreateEdgeConnections which does the heavy lifting for edge creation
 * (faster version of create edge connections internal for use at runtime)
 * @param Edges - list of polygon line segments to use for creation of saved/real Edge Connections
 * @param bTest - whether this is a test only 
 * @param bCrossPylonEdgesOnly - if TRUE only cross pylon edges will be created 
 * @param PolyObstMap - optional map input dictating relationships between polys in this mesh and obstacles (used during internal geo split edge creation)
 * @param out_Edges - optional output array of found edges (when supplied actual edges will not be created, instead this array is populated)
 * @param EdgeDetlaOverride - allows override of MAX_EDGE_DELTA 
 * @param MinEdgeLengthOverride - allows override of MIN_EDGE_LENGTH
 */
// square(3.5)
static FLOAT RuntimeEdgeVertToleranceSq = 12.25f;
#define RUNTIME_EDGE_VERT_TOLERANCE_SQ RuntimeEdgeVertToleranceSq
void UNavigationMeshBase::CreateEdgeConnectionsInternalFast( TArray<FNavMeshEdgeBase>& Edges,
														UBOOL bTest/* = FALSE*/,
														UBOOL bCrossPylonEdgesOnly/* = FALSE*/,
														UBOOL bAddDynamicEdges/* = FALSE*/,
														PolyToObstacleMap* PolyObstMap/* = NULL*/,
														TArray<FEdgeTuple>* out_Edges/* = NULL*/,
														FLOAT EdgeDeltaOverride/*=-1.f*/,
														FLOAT MinEdgeLengthOverride/*=-1.f*/)
{
	//SCOPE_QUICK_TIMERX(CreateEdgeConnectsIntern)

	if( Edges.Num() == 0 )
	{
		return;
	}

	INT Cnt = 0;
	UBOOL bDone = FALSE;
	APylon* Pylon = Cast<APylon>(GetOuter());

	FLOAT MinEdgeLength = (MinEdgeLengthOverride>-1.f) ? MinEdgeLengthOverride : MIN_EDGE_LENGTH;

	
	static TArray<FVector> VertLocationCache;
	VertLocationCache.Reset();

#define CACHE_STRIDE 4
	VertLocationCache.AddItem(Edges(0).GetVertLocation(0));
	VertLocationCache.AddItem(Edges(0).GetVertLocation(1));
	VertLocationCache.AddItem(ConditionalRemoveZ(VertLocationCache(0), !Pylon->bImportedMesh));
	VertLocationCache.AddItem(ConditionalRemoveZ(VertLocationCache(1), !Pylon->bImportedMesh));


	// Loop through edges
	// Check if smaller edge is along the big edge and divide up the edge
	for( INT OuterIdx = 0; OuterIdx < Edges.Num(); OuterIdx++ )
	{
		FNavMeshEdgeBase& OuterEdge = Edges(OuterIdx);

		if ( VertLocationCache.Num() <= CACHE_STRIDE*OuterIdx )
		{
			VertLocationCache.AddItem(OuterEdge.GetVertLocation(0));
			VertLocationCache.AddItem(OuterEdge.GetVertLocation(1));
			VertLocationCache.AddItem(ConditionalRemoveZ(VertLocationCache(OuterIdx*CACHE_STRIDE), !Pylon->bImportedMesh));
			VertLocationCache.AddItem(ConditionalRemoveZ(VertLocationCache(OuterIdx*CACHE_STRIDE+1), !Pylon->bImportedMesh));

		}

		if(OuterEdge.NavMesh != this)
		{
			continue;
		}

		FVector OuterEdgeVert0 = VertLocationCache(OuterIdx*CACHE_STRIDE);
		FVector OuterEdgeVert1 = VertLocationCache(OuterIdx*CACHE_STRIDE+1);

		FVector OuterEdgeVert0NoZ = VertLocationCache(OuterIdx*CACHE_STRIDE+2);
		FVector OuterEdgeVert1NoZ = VertLocationCache(OuterIdx*CACHE_STRIDE+3);


		// list of edges added for this segment (PRE sorted based on distance to poly0 in segment)
		TDoubleLinkedList<WORD> AddedEdges;

		for( INT InnerIdx = 0; InnerIdx < Edges.Num(); InnerIdx++ )
		{
			FNavMeshEdgeBase& InnerEdge = Edges(InnerIdx);
			if ( VertLocationCache.Num() <= CACHE_STRIDE*InnerIdx )
			{
				VertLocationCache.AddItem(InnerEdge.GetVertLocation(0));
				VertLocationCache.AddItem(InnerEdge.GetVertLocation(1));
				VertLocationCache.AddItem(ConditionalRemoveZ(VertLocationCache(InnerIdx*CACHE_STRIDE), !Pylon->bImportedMesh));
				VertLocationCache.AddItem(ConditionalRemoveZ(VertLocationCache(InnerIdx*CACHE_STRIDE+1), !Pylon->bImportedMesh));

			}


			// if we're checking cross-pylon edges and these edges are in the same mesh, skipo it!
			if(bCrossPylonEdgesOnly && InnerEdge.NavMesh == OuterEdge.NavMesh)
			{
				continue;
			}

			// Skip if it's the same edge
			if( InnerIdx == OuterIdx )
			{
				continue;
			}

			FNavMeshPolyBase* PolyInner = InnerEdge.BuildTempEdgePolys(0);
			FNavMeshPolyBase* PolyOuter = OuterEdge.BuildTempEdgePolys(0);
			// if this is somehow an edge from the same poly, skip it
			if(PolyInner == PolyOuter)
			{
				continue;
			}

			// if the polys are already connected, bail
			if (out_Edges == NULL && ArePolysConnected(PolyInner,PolyOuter))
			{
				continue;
			}


			FVector InnerEdgeVert0 = VertLocationCache(InnerIdx*CACHE_STRIDE);
			FVector InnerEdgeVert1 = VertLocationCache(InnerIdx*CACHE_STRIDE+1);

			FVector InnerEdgeVert0NoZ = VertLocationCache(InnerIdx*CACHE_STRIDE+2);
			FVector InnerEdgeVert1NoZ = VertLocationCache(InnerIdx*CACHE_STRIDE+3);

			// find closest points on outer edge to inner edge verts 
			// Vert 0 of inner edge, mapped to outer edge
			FLOAT T_vert0=0.f;
			FVector InnerVert0ToOuterEdge(0.f);
			FLOAT Vert0_Dist = SqPointDistToSegmentOutT(InnerEdgeVert0NoZ,OuterEdgeVert0NoZ,OuterEdgeVert1NoZ,InnerVert0ToOuterEdge,T_vert0);
			// Vert 1 of inner edge, mapped to outer edge
			FLOAT T_vert1=0.f;
			FVector InnerVert1ToOuterEdge(0.f);
			FLOAT Vert1_Dist = SqPointDistToSegmentOutT(InnerEdgeVert1NoZ,OuterEdgeVert0NoZ,OuterEdgeVert1NoZ,InnerVert1ToOuterEdge,T_vert1);


			FLOAT EdgeLen = (InnerVert0ToOuterEdge-InnerVert1ToOuterEdge).Size();
			if(EdgeLen < MinEdgeLength)
			{
				continue;
			}


			// now map the closest points on outer back to inner edge so we can check height deltas
			FLOAT T_renap_vert0=0.f;
			FVector ClosestPtOnInnerEdgeToMappedPoint0(0.f);
			FLOAT OtherVert0_Dist = SqPointDistToSegmentOutT(InnerVert0ToOuterEdge,InnerEdgeVert0NoZ,InnerEdgeVert1NoZ,ClosestPtOnInnerEdgeToMappedPoint0,T_renap_vert0);

			FLOAT T_renap_vert1=0.f;
			FVector ClosestPtOnInnerEdgeToMappedPoint1(0.f);
			FLOAT OtherVert1_Dist = SqPointDistToSegmentOutT(InnerVert1ToOuterEdge,InnerEdgeVert0NoZ,InnerEdgeVert1NoZ,ClosestPtOnInnerEdgeToMappedPoint1,T_renap_vert1);

			// see if the two edges are near each other
			if ( Vert0_Dist > RUNTIME_EDGE_VERT_TOLERANCE_SQ && Vert1_Dist > RUNTIME_EDGE_VERT_TOLERANCE_SQ && OtherVert0_Dist > RUNTIME_EDGE_VERT_TOLERANCE_SQ && OtherVert1_Dist > RUNTIME_EDGE_VERT_TOLERANCE_SQ )
			{
				continue;
			}

			// if this is not an imported mesh, restore Z value to closest points found
			if( !Pylon->bImportedMesh )
			{
				FVector OuterDelta = (OuterEdgeVert1 - OuterEdgeVert0);
				InnerVert0ToOuterEdge = OuterEdgeVert0 + (OuterDelta * T_vert0);
				InnerVert1ToOuterEdge = OuterEdgeVert0 + (OuterDelta * T_vert1);

				FVector InnerDelta = (InnerEdgeVert1 - InnerEdgeVert0);
				ClosestPtOnInnerEdgeToMappedPoint0 = InnerEdgeVert0 + (InnerDelta * T_renap_vert0);
				ClosestPtOnInnerEdgeToMappedPoint1 = InnerEdgeVert0 + (InnerDelta * T_renap_vert1);
			}

			// if the mapped points on outer edge are the same point, then this isn't a valid candidate
			if((InnerVert0ToOuterEdge-InnerVert1ToOuterEdge).SizeSquared() < MinEdgeLength*MinEdgeLength)
			{
				continue;
			}

			FVector Delta0 = ClosestPtOnInnerEdgeToMappedPoint0 - InnerVert0ToOuterEdge;
			FVector Delta1 = ClosestPtOnInnerEdgeToMappedPoint1 - InnerVert1ToOuterEdge;

			if(!Pylon->bImportedMesh)
			{
				Delta0.Z = 0.f;
				Delta1.Z = 0.f;
			}

			FLOAT MaxEdgeDelta = EdgeDeltaOverride;
			if(MaxEdgeDelta < 0.f)
			{
				MaxEdgeDelta=NAVMESHGEN_MAX_EDGE_DELTA;
			}
			if( Delta0.SizeSquared() > MaxEdgeDelta*MaxEdgeDelta||
				Delta1.SizeSquared() > MaxEdgeDelta*MaxEdgeDelta)
			{
				continue;
			}

			FLOAT Vert0ZDelta = Abs<FLOAT>(ClosestPtOnInnerEdgeToMappedPoint0.Z - InnerVert0ToOuterEdge.Z);
			FLOAT Vert1ZDelta = Abs<FLOAT>(ClosestPtOnInnerEdgeToMappedPoint1.Z - InnerVert1ToOuterEdge.Z);

			if( Vert0ZDelta < NAVMESHGEN_MAX_STEP_HEIGHT && Vert1ZDelta < NAVMESHGEN_MAX_STEP_HEIGHT )
			{
				static TArray<FNavMeshPolyBase*> ConnectedPolys;
				ConnectedPolys.Reset(2);

				ConnectedPolys.Append(OuterEdge.BuildTempEdgePolys);
				ConnectedPolys.Append(InnerEdge.BuildTempEdgePolys);

				// see if this edge is between a poly which is part of an obstacle splitting the mesh
				IInterface_NavMeshPathObstacle* Poly0Obstacle=NULL,*Poly1Obstacle=NULL;
				if(PolyObstMap != NULL)
				{
					Poly0Obstacle = ConnectedPolys(0)->NavMesh->SubMeshPolyIDToLinkeObstacleMap.FindRef(ConnectedPolys(0));	
					Poly1Obstacle = ConnectedPolys(1)->NavMesh->SubMeshPolyIDToLinkeObstacleMap.FindRef(ConnectedPolys(1));	
				}

				if(!InnerVert0ToOuterEdge.Equals(InnerVert1ToOuterEdge,GRIDSIZE))
				{
					if(out_Edges != NULL)
					{
						out_Edges->AddItem(FEdgeTuple(InnerVert0ToOuterEdge,
							InnerVert1ToOuterEdge,
							ConnectedPolys(0),
							OuterEdge.Vert0,
							OuterEdge.Vert1,
							ConnectedPolys(1),
							InnerEdge.Vert0,
							InnerEdge.Vert1,
							&OuterEdge,
							&InnerEdge));
					}
					else if( !BuildEdgesFromSupportedWidths(InnerVert0ToOuterEdge,
						InnerVert1ToOuterEdge,
						ConnectedPolys,
						bAddDynamicEdges,
						Poly0Obstacle,
						Poly1Obstacle,
						&OuterEdge,
						&InnerEdge))
					{
						return;
					}
				}
			}
		}
	}
}

UBOOL AlreadyInList(const TArray<FNavMeshEdgeBase>& Edges, VERTID Vert0, VERTID Vert1, FNavMeshPolyBase* Poly) 
{
	for(INT EdgeIdx=0;EdgeIdx<Edges.Num();++EdgeIdx)
	{
		const FNavMeshEdgeBase& Inner_Edge = Edges(EdgeIdx);
		if( Inner_Edge.NavMesh == Poly->NavMesh)
		{
			if( (( Inner_Edge.Vert0 == Vert0 && Inner_Edge.Vert1 == Vert1 ) || 
				( Inner_Edge.Vert1 == Vert0 && Inner_Edge.Vert0 == Vert1 ) ) && Inner_Edge.BuildTempEdgePolys.ContainsItem(Poly))
			{
				return TRUE;
			}
		}
	}
	
	return FALSE;
}

/** 
 * AddTempEdgesForPoly - adds temporary "edges" for each poly for each linesegment the polygon makes up
 *						 Used during edge creation
 * @param Poly - poly to add temp edges for
 * @param Edges - output array to add edges to
 * @param TestBox - optional, if provided only edges which intersect with this box will be added
 * @param bWorldSpace - TRUE if we are working in local space
 */
void UNavigationMeshBase::AddTempEdgesForPoly( FNavMeshPolyBase& Poly, TArray<FNavMeshEdgeBase>& Edges, const FBox* TestBox/*=NULL*/, UBOOL bWorldSpace/*=WORLD_SPACE*/)
{
	if(TestBox != NULL)
	{
		for( INT VertIdx = 0; VertIdx < Poly.PolyVerts.Num(); VertIdx++ )
		{			
			INT NextVertIdx = (VertIdx+1)%Poly.PolyVerts.Num();
			const FVector EdgeLoc0 = Poly.GetVertLocation(VertIdx,bWorldSpace);
			const FVector EdgeLoc1 = Poly.GetVertLocation(NextVertIdx,bWorldSpace);
			const FVector Norm = ((EdgeLoc0-EdgeLoc1)^FVector(0.f,0.f,1.f)).SafeNormal();
			const FPlane DaPlane(EdgeLoc0,Norm);
			if(FPlaneAABBIsect(DaPlane,*TestBox) && ! AlreadyInList(Edges,Poly.PolyVerts(VertIdx),Poly.PolyVerts(NextVertIdx),&Poly))
			{
				INT AddedIdx = Edges.AddItem( FNavMeshEdgeBase(Poly.NavMesh, Poly.PolyVerts(VertIdx), Poly.PolyVerts(NextVertIdx)) );
				FNavMeshEdgeBase& AddedEdge = Edges(AddedIdx);
				AddedEdge.BuildTempEdgePolys.AddItem(&Poly);
			}
		}
	}
	else
	{
		for( INT VertIdx = 0; VertIdx < Poly.PolyVerts.Num(); VertIdx++ )
		{			
			INT NextVertIdx = (VertIdx+1)%Poly.PolyVerts.Num();
			if( ! AlreadyInList(Edges,Poly.PolyVerts(VertIdx),Poly.PolyVerts(NextVertIdx),&Poly) )
			{
				INT AddedIdx = Edges.AddItem( FNavMeshEdgeBase(Poly.NavMesh, Poly.PolyVerts(VertIdx), Poly.PolyVerts(NextVertIdx)) );
				FNavMeshEdgeBase& AddedEdge = Edges(AddedIdx);
				AddedEdge.BuildTempEdgePolys.AddItem(&Poly);
			}
		}
	}
}

/** The octree semantics for polys. */
struct FNavPolyOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	static FBoxCenterAndExtent GetBoundingBox(FNavMeshPolyBase* Poly)
	{
		return FBoxCenterAndExtent(Poly->BoxBounds.GetCenter(), Poly->BoxBounds.GetExtent());
	}

	static void SetElementId(FNavMeshPolyBase* Poly, FOctreeElementId Id)
	{
		Poly->OctreeId = Id;
	}
};


void UNavigationMeshBase::MergeDropDownMesh()
{
	if(DropEdgeMesh == NULL)
	{
		return;
	}

	DropEdgeMesh->MergePolys(FVector(1.f,1.0f,0.f),FALSE);
	
	if(NAVMESHGEN_DRAW_DROPDOWN_POLYS)
	{
		for(PolyList::TIterator Itt(DropEdgeMesh->BuildPolys.GetHead());Itt;++Itt)
		{
			FNavMeshPolyBase* CurPoly = *Itt;
			CurPoly->DrawPoly(GWorld->PersistentLineBatcher,FColor(255,0,0));
		}
	}
}

/**
 * builds edges to top level polys (to adjacent submeshes) that were just cleared of obstacles
 * -- this is necessary because we might have had a submesh in the poly when other submeshes were added to adjacent polys
 *    that means the submeshes would be linked to the old submesh that was just removed and not to the top level poly
 *    in place now.  when this happens we need to relink to the top level poly
 * @param PolyIdx - id of the poly to build submesh edges for
 * @return - TRUE If successful 
 */
UBOOL UNavigationMeshBase::BuildSubMeshEdgesForJustClearedTLPoly(WORD PolyIdx)
{

	FNavMeshPolyBase* CurPoly = &Polys(PolyIdx);

	// *** Add edges for this submesh alone */
	// Make a sorted (longest->shortest) list of all poly edges (within this poly)
	static TArray<FNavMeshEdgeBase> SortedEdges;
	SortedEdges.Reset();

	AddTempEdgesForPoly(*CurPoly,SortedEdges);
	
	INT lastBorderIdx = SortedEdges.Num() - 1;


	// *** find cross pylon edges and disable obstacle geo associated with them if any so that when we build edges 
	//     we get an accurate representation of the world state
	// -> loop through all edges of top level poly and if it's cross pylon and linked to obstacle geo, add MAXWORD
	//    edge to list to indicate it should be disabled
	// list of obstacle mesh polys whose collision we've temprarily disabled
	static TArray<FNavMeshPolyBase*> ObstaclePolysWeHaveMessedWith;
	ObstaclePolysWeHaveMessedWith.Reset();
	ChangeObstacleMeshCollisionForCrossPylonEdges(PolyIdx,ObstaclePolysWeHaveMessedWith,FALSE);

	// for each edge in the top level poly, check if there is a submesh.. if so try and link to it
	for(INT EdgeIdx=0;EdgeIdx<CurPoly->GetNumEdges();++EdgeIdx)
	{
		FNavMeshEdgeBase* CurEdge = CurPoly->GetEdgeFromIdx(EdgeIdx,NULL,TRUE);

		if(CurEdge == NULL || !CurEdge->ShouldBeConsideredForObstacleGeo())
		{
			continue;
		}

		FNavMeshPolyBase* OtherPoly = CurEdge->GetOtherPoly(CurPoly);

		if(OtherPoly == NULL)
		{
			continue;
		}		

		if( lastBorderIdx < SortedEdges.Num()-1 )
		{
			SortedEdges.RemoveSwap(lastBorderIdx+1,SortedEdges.Num()-lastBorderIdx-1);
		}

		// if the other poly has a submesh, try to link to it
		if(OtherPoly->NumObstaclesAffectingThisPoly>0)
		{
			// ensure opposing submesh is up to date
			FPolyObstacleInfo* Info = OtherPoly->GetObstacleInfo();

			if( Info != NULL )
			{
				UNavigationMeshBase* OtherPolySubMesh = Info->SubMesh;

				checkSlowish(OtherPolySubMesh != NULL);
				if( OtherPolySubMesh != NULL )
				{
					static TArray<FNavMeshPolyBase*> SubmeshPolys;
					SubmeshPolys.Reset();

					FBox TransformedBounds = CurPoly->GetPolyBounds();
					OtherPolySubMesh->GetIntersectingPolys(TransformedBounds.GetCenter(),TransformedBounds.GetExtent()+FVector(NAVMESHGEN_STEP_SIZE),SubmeshPolys,WORLD_SPACE);

					for(INT SubmeshIdx=0;SubmeshIdx<SubmeshPolys.Num();++SubmeshIdx)
					{
						FNavMeshPolyBase* Poly = SubmeshPolys(SubmeshIdx);
						checkSlowish(Poly!=NULL);
						AddTempEdgesForPoly(*Poly,SortedEdges,&TransformedBounds);
					}
				}
			}
			CreateEdgeConnectionsInternalFast(SortedEdges,FALSE,TRUE,TRUE,&SubMeshPolyIDToLinkeObstacleMap);
		}

		UNavigationMeshBase* OtherPolySubmesh = OtherPoly->GetSubMesh();
		if( OtherPolySubmesh != NULL )
		{
			CurEdge->PostSubMeshUpdateForOwningPoly(OtherPoly,OtherPolySubmesh,TRUE);
		}
		
	}


	// undo collision disabling on obstacle mesh polys
	ChangeObstacleMeshCollisionForCrossPylonEdges(PolyIdx,ObstaclePolysWeHaveMessedWith,TRUE);

	return TRUE;
}
/**
* builds edges for a submesh of a given polygon
* @param PolyIdx - id of the poly to build submesh edges for
* @return - TRUE If successful 
*/
extern UBOOL IsPointOnSegment(const FVector& Point, const FVector& SegPt0, const FVector& SegPt1, UBOOL bEndPointExclusive);

/**
 * marks obstacle geo associated with cross pylon edges on the passed poly as disabled so that edge generation can commence
 * @param PolyID - ID associated with the poly whose edges we are looking at 
 * @param out_AffectedPolys - list of obstacle mesh polys affected by the change
 * @param bNewCollisionState - whether to disable or enable collision (TRUE indicates we should turn collision back on)
 */
void UNavigationMeshBase::ChangeObstacleMeshCollisionForCrossPylonEdges(WORD PolyID, TArray<FNavMeshPolyBase*>& out_AffectedPolys, UBOOL bNewCollisionState)
{
	// if we're turning collision back on just loop through all affected polys and remove the flag from the list
	if( bNewCollisionState )
	{
		for(INT PolyIdx=0;PolyIdx<out_AffectedPolys.Num();++PolyIdx)
		{
			FNavMeshPolyBase* CurPoly = out_AffectedPolys(PolyIdx);
			if( CurPoly != NULL )
			{
				CurPoly->PolyEdges.RemoveItem(MAXWORD);
			}
		}

		return;
	}

	FNavMeshPolyBase* CurPoly = GetPolyFromId(PolyID);
	
	for(INT EdgeIdx=0;EdgeIdx<CurPoly->GetNumEdges();++EdgeIdx)
	{
		FNavMeshEdgeBase* CurEdge = CurPoly->GetEdgeFromIdx(EdgeIdx,NULL,TRUE);
		if(CurEdge == NULL || !CurEdge->ShouldBeConsideredForObstacleGeo())
		{
			continue;
		}


		// if this is a cross pylon edge disable any obstacle geo associated with it so we can properly build edge widths
		if( CurEdge->IsCrossPylon() )
		{
			FNavMeshCrossPylonEdge* CPEdge = static_cast<FNavMeshCrossPylonEdge*>(CurEdge);

			FNavMeshCrossPylonEdge* Edges[2];
			Edges[0] = CPEdge;
			Edges[1] = NULL;

			FNavMeshPolyBase* Poly0 = CPEdge->GetPoly0();
			FNavMeshPolyBase* Poly1 = CPEdge->GetPoly1();

			// get accompanying edge 
			FNavMeshEdgeBase* OtherEdge = NULL;

			if( Poly0 != NULL && Poly1 != NULL )
			{
				if( Poly0->NavMesh == CPEdge->NavMesh )
				{
					OtherEdge = Poly1->GetEdgeTo(Poly0,TRUE);
				}
				else if( Poly1->NavMesh == CPEdge->NavMesh )
				{
					OtherEdge = Poly0->GetEdgeTo(Poly1,TRUE);
				}
			}

			if( OtherEdge!=NULL && OtherEdge->IsCrossPylon())
			{
				Edges[1] = static_cast<FNavMeshCrossPylonEdge*>(OtherEdge);
			}

			for(INT eIdx=0;eIdx<2;++eIdx)
			{
				FNavMeshCrossPylonEdge* eEdge = Edges[eIdx];
				if( eEdge != NULL && eEdge->ObstaclePolyID != MAXWORD )
				{
					UNavigationMeshBase* ObstacleMeshForCPEdge = eEdge->NavMesh->GetObstacleMesh();

					if ( ObstacleMeshForCPEdge != NULL )
					{
						FNavMeshPolyBase* ObstacleMeshPoly = ObstacleMeshForCPEdge->GetPolyFromId(eEdge->ObstaclePolyID);
						// mark it as disabled collision
						ObstacleMeshPoly->PolyEdges.AddItem(MAXWORD);
						out_AffectedPolys.AddItem(ObstacleMeshPoly);
					}
				}
			}

		}
	}
}

UBOOL IsPointCloseToOnSegment(const FVector& Point, const FVector& SegPt0, const FVector& SegPt1, UBOOL bEndPointExclusive, FLOAT Tolerance=-1.0f)
{
	if( Tolerance < 0.f )
	{
		Tolerance = EDGE_VERT_TOLERANCE;
	}
	FLOAT ToleranceSq = Tolerance*Tolerance;
	FVector Closest(0.f);
	FLOAT Dist = PointDistToSegment(Point,SegPt0,SegPt1,Closest);
	if (bEndPointExclusive)
	{
		if( (Closest-SegPt0).IsNearlyZero() || (Closest-SegPt1).IsNearlyZero() )
		{
			return FALSE;
		}
	}
	return ( Dist < Tolerance || ((Closest-Point).SizeSquared2D() < ToleranceSq && Abs<FLOAT>(Closest.Z - Point.Z) < NAVMESHGEN_MAX_STEP_HEIGHT) );
}

/**
 * DoEdgesOverLap 
 * @param A0 - point 0 of edge A
 * @param A1 - point 1 of Edge A
 * @param B0 - point 0 of Edge B
 * @param B1 - point 1 of Edge B
 * @return TRUE if edges might overlap each other 
 */
UBOOL DoEdgesOverLap(const FVector& A0, const FVector& A1, const FVector& B0, const FVector& B1 )
{
	// project edge B onto A and see how much overlap there is (if any)
	const FVector A_Delta = A1 - A0;
	const FLOAT A_Len = A_Delta.Size();
	const FVector A_Dir = A_Delta/A_Len;

	
	// project B's points onto A
	const FLOAT B0_ProjA = (B0-A0) | A_Dir;
	const FLOAT B1_ProjA = (B1-A0) | A_Dir;
	
	if( B0_ProjA > -EDGE_VERT_TOLERANCE && B0_ProjA < A_Len + EDGE_VERT_TOLERANCE )
	{
		if( B1_ProjA > -EDGE_VERT_TOLERANCE && B1_ProjA < A_Len + EDGE_VERT_TOLERANCE )
		{
			return TRUE;
		}
	}

	return FALSE;
}

extern UBOOL PointsEqualEnough(const FVector& PtA, const FVector& PtB, FLOAT Tolerance);

static UBOOL EdgesAreEqualEnough(const FVector& Edge0_Vert0,const FVector& Edge0_Vert1,const FVector& Edge1_Vert0,const FVector& Edge1_Vert1, FLOAT Tolerance=5.0f)
{
	UBOOL bVert0Matches = PointsEqualEnough(Edge0_Vert0,Edge1_Vert0,Tolerance) || PointsEqualEnough(Edge0_Vert0,Edge1_Vert1,Tolerance);
	UBOOL bVert1Matches = PointsEqualEnough(Edge0_Vert1,Edge1_Vert0,Tolerance) || PointsEqualEnough(Edge0_Vert1,Edge1_Vert1,Tolerance);

	return bVert0Matches && bVert1Matches;
}

UBOOL UNavigationMeshBase::BuildSubMeshEdgesForPoly(WORD PolyIdx, const TArray<FPolyObstacleInfo*>& ObstaclesThatWereJustBuilt)
{
#define DO_SUBMESHEDGES_PERF 0
#if DO_SUBMESHEDGES_PERF 
	static UBOOL bDoIt = FALSE;
	if( bDoIt == TRUE )
	{
		GCurrentTraceName = NAME_Game;
	}
	else
	{
		GCurrentTraceName = NAME_None;
	}

	appStartCPUTrace( NAME_Game, FALSE, TRUE, 40, NULL );
#endif

	FPolyObstacleInfo* Info = PolyObstacleInfoMap.Find(PolyIdx);

	if(Info == NULL || Info->SubMesh == NULL)
	{
		// if we have no obstacles affecting us, bail!
		return FALSE;
	}
	else if(Info->bNeedRecompute == FALSE)
	{
		return FALSE;
	}

	UNavigationMeshBase * NewMesh = Info->SubMesh;
	FNavMeshPolyBase* CurPoly = GetPolyFromId(PolyIdx);

	NewMesh->FlushEdges();

	// *** Add edges for this submesh alone */
	static TArray<FNavMeshEdgeBase> SortedEdges;
	SortedEdges.Reset();

	for( INT SubPolyIdx = 0; SubPolyIdx  < NewMesh->Polys.Num(); ++SubPolyIdx )
	{
		AddTempEdgesForPoly(NewMesh->Polys(SubPolyIdx),SortedEdges);
	}

	NewMesh->CreateEdgeConnectionsInternalFast(SortedEdges,FALSE,FALSE,FALSE,&NewMesh->SubMeshPolyIDToLinkeObstacleMap);
	NewMesh->StaticVertCount = NewMesh->Verts.Num();
	
	// add border edges of the submesh
	INT lastBorderIdx = 0;
	if( SortedEdges.Num() > 0 )
	{
		lastBorderIdx = SortedEdges.Num() - 1;

		// build a list of edge data such that we have one dummy edge per adjacent polygon, with verts of this poly's actual geo and not the previous edges
		// this is so we don't do extra work for each edge connecting adjacent polys since there could be many (for differing edge sizes)
		static TArray<FNavMeshEdgeBase> OuterEdges;
		OuterEdges.Reset();
		for(INT VertIdx=0;VertIdx<CurPoly->PolyVerts.Num();++VertIdx)
		{
			INT NextVertIdx = (VertIdx+1) % CurPoly->PolyVerts.Num();
			VERTID Vert0 = CurPoly->PolyVerts(VertIdx);
			VERTID Vert1 = CurPoly->PolyVerts( NextVertIdx );
			FVector SegmentEndPoint0 = CurPoly->GetVertLocation(VertIdx,LOCAL_SPACE);
			FVector SegmentEndPoint1 = CurPoly->GetVertLocation(NextVertIdx,LOCAL_SPACE);

			for(INT EdgeIdx=0;EdgeIdx<CurPoly->GetNumEdges();++EdgeIdx)
			{
				FNavMeshEdgeBase* CurEdge = CurPoly->GetEdgeFromIdx(EdgeIdx,NULL,TRUE);
				if(CurEdge == NULL || !CurEdge->ShouldBeConsideredForObstacleGeo() || CurEdge->IsOneWayEdge())
				{
					continue;
				}

				FNavMeshPolyBase* OtherPoly = CurEdge->GetOtherPoly(CurPoly);

				if(OtherPoly == NULL)
				{
					continue;
				}		

				// if current edge is on poly segment, check to see if we have a link to target poly yet
				
				FVector Closest(0.f);
				if( DoEdgesOverLap(SegmentEndPoint0,SegmentEndPoint1,CurEdge->GetVertLocation(0,LOCAL_SPACE),CurEdge->GetVertLocation(1,LOCAL_SPACE)) )
				{
					
					UBOOL bAlreadyHaveDummyLinkToPoly = FALSE;
					for(INT OuterEdgeIdx=OuterEdges.Num()-1;OuterEdgeIdx>=0;--OuterEdgeIdx)
					{
						FNavMeshEdgeBase& CurOuterEdge = OuterEdges(OuterEdgeIdx);
						if( CurOuterEdge.BuildTempEdgePolys(0) == OtherPoly && CurOuterEdge.EdgeGroupID == CurEdge->EdgeGroupID)
						{
							// we already have a dummy edge with these verts, that links to this poly.. so bail
							bAlreadyHaveDummyLinkToPoly=TRUE;
							break;
						}
					}

					// if we don't have a link to this poly yet, we need to find the best candidate from this edge group (largest edge)
					if( !bAlreadyHaveDummyLinkToPoly )
					{
						static TArray<FNavMeshEdgeBase*> EdgesInGroup;
						EdgesInGroup.Reset();

						CurEdge->GetAllStaticEdgesInGroup(CurPoly,EdgesInGroup);
						FLOAT BestLen = 0.f;
						INT BestIdx=-1;
						for(INT EdgeGroupIdx=0;EdgeGroupIdx<EdgesInGroup.Num();++EdgeGroupIdx)
						{
							FNavMeshEdgeBase* CurEdgeInGroup = EdgesInGroup(EdgeGroupIdx);
							FLOAT CurLen = CurEdgeInGroup->GetEdgeLength();
							if( CurLen > BestLen )
							{
								BestLen = CurLen;
								BestIdx = EdgeGroupIdx;
							}
						}

						// if we found the winner edge, save off dummy edge with its verts
						if( BestIdx >= 0 )
						{
							FNavMeshEdgeBase* LongestEdgeInGroup = EdgesInGroup(BestIdx);
							VERTID AddVIdx0 = LongestEdgeInGroup->Vert0;
							VERTID AddVIdx1 = LongestEdgeInGroup->Vert1;
							const FVector V0Loc = LongestEdgeInGroup->GetVertLocation(0,WORLD_SPACE);
							const FVector V1Loc = LongestEdgeInGroup->GetVertLocation(1,WORLD_SPACE);

							// one more step.. if polys are in different meshes we need to find VERTIDs in the other mesh that match
							// the verts in CurPoly's mesh
							if( OtherPoly->NavMesh != CurPoly->NavMesh )
							{
								UBOOL bFoundMatchingEdge=FALSE;
								// loop through each edge in OtherPoly looking for one that has identical verts (there should be at least one!)
								FNavMeshEdgeBase* OtherPolyEdge = NULL;
								for(INT OtherPolyEdgeIdx=0;OtherPolyEdgeIdx<OtherPoly->GetNumEdges();++OtherPolyEdgeIdx)
								{
									OtherPolyEdge = OtherPoly->GetEdgeFromIdx(OtherPolyEdgeIdx,NULL,TRUE);
									if( OtherPolyEdge != NULL && EdgesAreEqualEnough(OtherPolyEdge->GetVertLocation(0,WORLD_SPACE),OtherPolyEdge->GetVertLocation(1,WORLD_SPACE),V0Loc,V1Loc) )
									{
										bFoundMatchingEdge=TRUE;
										AddVIdx0=OtherPolyEdge->Vert0;
										AddVIdx1=OtherPolyEdge->Vert1;
										break;
									}
								}


								if(!bFoundMatchingEdge)
								{
									continue;//derp
								}
							}


							INT AddedIdx = OuterEdges.AddItem( FNavMeshEdgeBase(OtherPoly->NavMesh,AddVIdx0,AddVIdx1));		
							FNavMeshEdgeBase& AddedEdge = OuterEdges(AddedIdx);
							AddedEdge.BuildTempEdgePolys.AddItem(OtherPoly);
							AddedEdge.EdgeGroupID = CurEdge->EdgeGroupID;
						}
					}
				}
			}
		}


		// at this point we should have a list of dummy edges that represent connections to adjacent top level polys
		// go through them all and try to build edges from our submesh to the adjacent poly, or the adjacent submesh
		for(INT OuterEdgeIdx=0;OuterEdgeIdx<OuterEdges.Num();++OuterEdgeIdx)
		{
			FNavMeshEdgeBase& CurOuterEdge = OuterEdges(OuterEdgeIdx);

			FNavMeshPolyBase* OtherPoly = CurOuterEdge.BuildTempEdgePolys(0);

			if( lastBorderIdx < SortedEdges.Num()-1 )
			{
				SortedEdges.RemoveSwap(lastBorderIdx+1,SortedEdges.Num()-lastBorderIdx-1);
			}

			// if the other poly has a submesh, link to the submesh instead of the Top level poly
			if(OtherPoly->NumObstaclesAffectingThisPoly>0)
			{
				// ensure opposing submesh is up to date
				FPolyObstacleInfo* Info = OtherPoly->GetObstacleInfo();

				// if this submesh wasn't just rebuilt we still need to do this even if bNeedsRecompute is false
				if( Info != NULL && (!ObstaclesThatWereJustBuilt.ContainsItem(Info) ||Info->bNeedRecompute))
				{
					UNavigationMeshBase* OtherPolySubMesh = Info->SubMesh;

					static TArray<FNavMeshPolyBase*> SubmeshPolys;
					SubmeshPolys.Reset();

					FBox TransformedBounds = CurPoly->GetPolyBounds(WORLD_SPACE);
					TransformedBounds = TransformedBounds.ExpandBy(EDGE_VERT_TOLERANCE);
					OtherPolySubMesh->GetIntersectingPolys(TransformedBounds.GetCenter(),TransformedBounds.GetExtent()+FVector(NAVMESHGEN_STEP_SIZE),SubmeshPolys,WORLD_SPACE);

					for(INT SubmeshIdx=0;SubmeshIdx<SubmeshPolys.Num();++SubmeshIdx)
					{
						FNavMeshPolyBase* Poly = SubmeshPolys(SubmeshIdx);
						checkSlowish(Poly!=NULL);
						AddTempEdgesForPoly(*Poly,SortedEdges,&TransformedBounds);
					}
				}

			}
			else
				// if this is a normal poly (doesn't have a submesh)
			{
				SortedEdges.AddItem( CurOuterEdge );		
			}

			NewMesh->CreateEdgeConnectionsInternalFast(SortedEdges,FALSE,TRUE,TRUE,&NewMesh->SubMeshPolyIDToLinkeObstacleMap,NULL,NAVMESHGEN_MAX_EDGE_DELTA*2.0f);
		}

	}

	NewMesh->PopulateEdgePtrCache();
	NewMesh->FlushEdgeStorageData();

#if DO_SUBMESHEDGES_PERF
	appStopCPUTrace( NAME_Game );
#endif
	return TRUE;
}

/**
 * builds a mapping from polys to the pathobject whose interior they're within
 * @param Mesh - the mesh we're building the mapping for
 * @param Map - the output map for the linkage between polys and path objects
 */
typedef UNavigationMeshBase::FMeshSplitingShape PS3CompilerFix;
// array of pathobjects currently in the level
APylon::PathObjectList PathObjects;
IMPLEMENT_COMPARE_CONSTREF(PS3CompilerFix, BigSplitsFirst, { return (FNavMeshPolyBase::CalcArea(A.Shape) < FNavMeshPolyBase::CalcArea(B.Shape)); })
void BuildPolyToPathObjectMap(UNavigationMeshBase* Mesh, PolyToPOMap& out_Map)
{
	// ** First, build a list of path objects which affect this mesh
	TArray<UNavigationMeshBase::FMeshSplitingShape> AffectingPOs;

	for(INT PathObjectIdx=0;PathObjectIdx<PathObjects.Num();++PathObjectIdx)
	{
		IInterface_NavMeshPathObject* CurrentPO = PathObjects(PathObjectIdx);
		UNavigationMeshBase::FMeshSplitingShape SplitShape;
		if(CurrentPO->GetMeshSplittingPoly(SplitShape.Shape,SplitShape.Height))
		{
			FNavMeshPolyBase* IntersectingPoly=NULL;
			if(Mesh->IntersectsPoly(SplitShape.Shape,IntersectingPoly,NULL,TRUE))
			{
				SplitShape.bKeepInternalGeo=TRUE;
				SplitShape.ID = PathObjectIdx;
				AffectingPOs.AddItem(SplitShape);
			}
		}
	}	

	/** sort list big first, so that small shapes have last say on poly ownership */
	Sort<USE_COMPARE_CONSTREF(PS3CompilerFix,BigSplitsFirst)>(&AffectingPOs(0),AffectingPOs.Num());

	for( INT PolyIdx = 0; PolyIdx < Mesh->Polys.Num(); ++PolyIdx )
	{
		FNavMeshPolyBase* CurPoly = &Mesh->Polys(PolyIdx);
		for(INT POIdx=0;POIdx<AffectingPOs.Num();++POIdx)
		{
			UNavigationMeshBase::FMeshSplitingShape& CurShape = AffectingPOs(POIdx);
			FVector Ctr = FVector(0.f);
			FBox Bounds(1);
			for(INT VertIdx=0;VertIdx<CurShape.Shape.Num();VertIdx++)
			{
				Bounds += CurShape.Shape(VertIdx);
				Bounds += CurShape.Shape(VertIdx)+FVector(0.f,0.f,CurShape.Height);
				Bounds += CurShape.Shape(VertIdx)+FVector(0.f,0.f,-NAVMESHGEN_ENTITY_HALF_HEIGHT);
				Ctr +=CurShape.Shape(VertIdx);
			}
			Ctr /= CurShape.Shape.Num();

			if( CurPoly->ContainsPoint( Ctr ) || ( FNavMeshPolyBase::ContainsPoint(CurShape.Shape,CurPoly->GetPolyCenter()) && Bounds.IsInside(CurPoly->GetPolyCenter())))
			{
				out_Map.Set(CurPoly,PathObjects(CurShape.ID));
			}
		}
	}
}

void UNavigationMeshBase::CreateEdgeConnections( UBOOL bTest )
{
	APylon* Pylon = Cast<APylon>(GetOuter());
	if( !bTest )
	{
		FlushEdges();
		// empty refs in obstacle mesh to edges
		UNavigationMeshBase* ObstacleMesh = GetObstacleMesh();
		if (ObstacleMesh != NULL)
		{
			for( INT PolyIdx=0;PolyIdx<ObstacleMesh->Polys.Num();++PolyIdx)
			{
				FNavMeshPolyBase* CurPoly = &ObstacleMesh->Polys(PolyIdx);
				CurPoly->PolyEdges.Empty();
			}
		}

	}
	
	// build a mapping from pathobjects to the polys inside them
	PolyToPOMap POMap;
	BuildPolyToPathObjectMap(this,POMap);
	GetObstacleMesh()->BuildObstacleGeoForPathobjects(this);

	// build drop down edges first, so that they get linked to obstacle mesh geo 
	static TArray<FEdgeRef> DropDownEdges;
	DropDownEdges.Reset();
	BuildDropDownEdges(DropDownEdges);

	TArray<FNavMeshEdgeBase> SortedEdges;

	// build cross pylon links first so when we are generating non-crosspylon links the proper obstacle mesh geo is disabled
	if(GetPylon()->NeedsStaticCrossPylonEdgesBuilt())
	{
		// add border edges
		for(INT BorderEdgeIdx=0;BorderEdgeIdx<BorderEdgeSegments.Num();BorderEdgeIdx++)
		{
			BorderEdgeInfo& Info = BorderEdgeSegments(BorderEdgeIdx);
			FNavMeshPolyBase* Poly = GetPolyFromId(Info.Poly);
			INT AddedIdx = SortedEdges.AddItem( FNavMeshEdgeBase(Poly->NavMesh,Info.Vert0,Info.Vert1 ) );
			FNavMeshEdgeBase& AddedEdge = SortedEdges(AddedIdx);
			AddedEdge.BuildTempEdgePolys.AddItem(Poly);
		}

		TArray<APylon*> Pylons;
		FBox PylonBounds = Pylon->GetBounds(WORLD_SPACE);

		UNavigationHandle::GetIntersectingPylons(PylonBounds.GetCenter(), PylonBounds.GetExtent(),Pylons);
		if(Pylons.Num() > 1)
		{
			for(INT IsectPylonIdx=0;IsectPylonIdx<Pylons.Num();IsectPylonIdx++)
			{
				APylon* CurPylon = Pylons(IsectPylonIdx);
				checkSlowish(CurPylon);

				if(CurPylon == Pylon ||
					CurPylon->NavMeshPtr == NULL ||
					!CurPylon->NeedsStaticCrossPylonEdgesBuilt() ||
					(CurPylon->bImportedMesh && !Pylon->bImportedMesh) ||
					CurPylon->ImposterPylons.ContainsItem(Pylon) || Pylon->ImposterPylons.ContainsItem(CurPylon))
				{
					continue;
				}


				TArray<FNavMeshPolyBase*> Polys;
				CurPylon->GetIntersectingPolys(PylonBounds.GetCenter(), PylonBounds.GetExtent()+FVector(NAVMESHGEN_STEP_SIZE),Polys,TRUE);
				for(INT PolyIdx=0;PolyIdx<Polys.Num();PolyIdx++)
				{
					FNavMeshPolyBase* Poly = Polys(PolyIdx);
					checkSlowish(Poly!=NULL);
					AddTempEdgesForPoly(*Poly,SortedEdges);
				}
			}

			// OK! so.. since there is going to be obstacle mesh geo along boundaries with other pylons we must first add
			// dummy edges to the pylons to disable the obstacle mesh geo along the adjacent edge 
			static TArray<FEdgeTuple> Edges;
			Edges.Reset();
			CreateEdgeConnectionsInternal(SortedEdges,bTest,TRUE,FALSE,&Edges,-1.0f,-1.0f);

			static TArray<FNavMeshPolyBase*> ConnectedPolys;

			// disable collision of obstacle mesh surfaces along boundary edges so we get accurate width generation
			for(INT EdgeIdx=0;EdgeIdx<Edges.Num();++EdgeIdx)
			{
				FEdgeTuple& EdgeTuple = Edges(EdgeIdx);
				ConnectedPolys.Reset(2);
				ConnectedPolys.AddItem(EdgeTuple.Poly0);
				ConnectedPolys.AddItem(EdgeTuple.Poly1);

				for(INT Idx=0;Idx<ConnectedPolys.Num();++Idx)
				{
					FNavMeshPolyBase* CurPoly = ConnectedPolys(Idx);
					UNavigationMeshBase* ObsMesh = CurPoly->NavMesh->GetObstacleMesh();
					const FVector LS_Entry = ObsMesh->W2LTransformFVector(EdgeTuple.Pt0);
					const FVector LS_Exit = ObsMesh->W2LTransformFVector(EdgeTuple.Pt1);

					if(ObsMesh != NULL && ObsMesh->Polys.Num() > 0)
					{
						for(INT PolyIdx=0;PolyIdx<ObsMesh->Polys.Num();PolyIdx++)
						{
							FNavMeshPolyBase& Poly = ObsMesh->Polys(PolyIdx);
							TryToLinkPolyToEdge(ObsMesh,Poly,LS_Entry,LS_Exit,MAXWORD,FALSE,NULL,FALSE);
						}
					}
				}
			}


			// now sweep the edge spans and add edges with proper supported widths
			for(INT EdgeIdx=0;EdgeIdx<Edges.Num();++EdgeIdx)
			{
				FEdgeTuple& EdgeTuple = Edges(EdgeIdx);
				ConnectedPolys.Reset(2);
				ConnectedPolys.AddItem(EdgeTuple.Poly0);
				ConnectedPolys.AddItem(EdgeTuple.Poly1);
				BuildEdgesFromSupportedWidths(EdgeTuple.Pt0,EdgeTuple.Pt1,ConnectedPolys,FALSE,NULL,NULL,EdgeTuple.OuterEdge,EdgeTuple.InnerEdge,&POMap);
			}

			// undo-disable collision of obstacle mesh surfaces along boundary edges so we get accurate width generation
			for(INT EdgeIdx=0;EdgeIdx<Edges.Num();++EdgeIdx)
			{
				FEdgeTuple& EdgeTuple = Edges(EdgeIdx);
				ConnectedPolys.Reset(2);
				ConnectedPolys.AddItem(EdgeTuple.Poly0);
				ConnectedPolys.AddItem(EdgeTuple.Poly1);

				for(INT Idx=0;Idx<ConnectedPolys.Num();++Idx)
				{
					FNavMeshPolyBase* CurPoly = ConnectedPolys(Idx);
					UNavigationMeshBase* ObsMesh = CurPoly->NavMesh->GetObstacleMesh();

					if(ObsMesh != NULL && ObsMesh->Polys.Num() > 0)
					{
						for(INT PolyIdx=0;PolyIdx<ObsMesh->Polys.Num();PolyIdx++)
						{
							FNavMeshPolyBase& Poly = ObsMesh->Polys(PolyIdx);
							Poly.PolyEdges.RemoveItem(MAXWORD);
						}
					}
				}
			}


		}

	}

	SortedEdges.Empty();

	// rebuild obstacle mesh to split where necessary for obstacle polys which are linked to edges (so we only disable the portion of the collision adjacent to the edge)
	GetObstacleMesh()->RebuildObstacleMeshGeometryForLinkedEdges(this);

	// once cross pylon edges and dropdown edges are complete, build internal edges!
	for( INT PolyIdx = 0; PolyIdx < Polys.Num(); PolyIdx++ )
	{
		AddTempEdgesForPoly(Polys(PolyIdx),SortedEdges);
	}

	CreateEdgeConnectionsInternal(SortedEdges,bTest,FALSE,FALSE,NULL,-1.0f,-1.0f,&POMap);

	SortedEdges.Empty();

	// ** find all the other intersecting pylons to this one and add possible inter-pylon link edges to the list
	if( FNavMeshWorld::GetPylonOctree() == NULL )
	{
		return;
	}

	if(GetPylon()->NavMeshPtr == this)
	{
		BuildBorderEdgeList();
	}	

	// loop through drop down edges and remove them from the obstacle mesh because we don't want them disabling collision at runtime
	// (we needed collision disabled during edge generation so that adjacent edges get built correctly, but that's all)
	for(INT DDidx=0;DDidx<DropDownEdges.Num();++DDidx)
	{
		FEdgeRef& Ref = DropDownEdges(DDidx);
		FNavMeshDropDownEdge* Edge = static_cast<FNavMeshDropDownEdge*>(Ref.Mesh->GetEdgeAtIdx(Ref.EdgeId));
		
		FNavMeshPolyBase* OBPoly = Edge->NavMesh->GetObstacleMesh()->GetPolyFromId(Edge->ObstaclePolyID);

		if( OBPoly != NULL )
		{
			for(INT OBPOlyEdgeIdx=0;OBPOlyEdgeIdx<OBPoly->PolyEdges.Num(); ++OBPOlyEdgeIdx) 
			{
				if( Edge->NavMesh->GetEdgeAtIdx(OBPoly->PolyEdges(OBPOlyEdgeIdx)) == Edge )
				{
					OBPoly->PolyEdges.Remove(OBPOlyEdgeIdx);
					Edge->ObstaclePolyID = MAXWORD;
					break;
				}
			}
		}
	}


	PopulateEdgePtrCache();
}


/**
 * creates and adds a dropdown edge to SrcPoly pointing to DestPoly 
 * @param SrcPoly - source poly of edge
 * @param DestPoly - destination poly of edge
 * @param StartPt - starting point of edge
 * @param EndPt   - end point of edge 
 */
FNavMeshDropDownEdge* AddDropDownEdge( FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, const FVector& StartPt, const FVector& EndPt,TArray<UNavigationMeshBase::FEdgeRef>& out_AddedEdges ) 
{
	TArray<FNavMeshPolyBase*> ConnectedPolyPolys;
	ConnectedPolyPolys.AddItem(SrcPoly);
	ConnectedPolyPolys.AddItem(DestPoly);

	FNavMeshDropDownEdge* NewEdge = NULL;
	INT EdgeId = 0;
	if(!SrcPoly->NavMesh->AddOneWayCrossPylonEdgeToMesh<FNavMeshDropDownEdge>(StartPt,EndPt,ConnectedPolyPolys,(StartPt-EndPt).Size(),MAXBYTE,&NewEdge,FALSE,TRUE,&EdgeId))
	{
		return NULL;
	}

	if(NewEdge != NULL)
	{
		// find maxhieght delta between polyverts, as that is the drop height detected here
		FLOAT BiggestDelta = 0.f;
		for(INT VertIdx=0;VertIdx<SrcPoly->PolyVerts.Num();++VertIdx)
		{
			FLOAT ThisVertHeight = SrcPoly->GetVertLocation(VertIdx,LOCAL_SPACE).Z;
			FLOAT NextVertHeight = SrcPoly->GetVertLocation((VertIdx+1)%SrcPoly->PolyVerts.Num(),LOCAL_SPACE).Z;
			FLOAT ThisDelta = Abs<FLOAT>(ThisVertHeight-NextVertHeight);
			if(ThisDelta > BiggestDelta)
			{
				BiggestDelta=ThisDelta;
			}
		}

		NewEdge->DropHeight=BiggestDelta;

		// try to link edge to obstacle mesh, we do this here so that collision on obstacle mesh surfaces
		// adjacent to the drop down edges are disabled during edge creation and adjacent polys will link into potentially narrow ones 
		// next to the drop off
		UNavigationMeshBase* ObstMesh = NewEdge->NavMesh->GetObstacleMesh();
		if ( ObstMesh != NULL )
		{
			for(INT PolyIdx=0; PolyIdx< ObstMesh->Polys.Num(); ++ PolyIdx)
			{
				FNavMeshPolyBase& Poly = ObstMesh->Polys(PolyIdx);

				TryToLinkPolyToEdge(ObstMesh,Poly,StartPt,EndPt,EdgeId,FALSE,NewEdge,FALSE);
			}
		}

		UNavigationMeshBase::FEdgeRef Ref;
		Ref.Mesh = NewEdge->NavMesh;
		Ref.EdgeId = EdgeId;
		out_AddedEdges.AddItem(Ref);

		return NewEdge;
	}

	return NULL;
}

/**
 * Determines if there is a clear trajectory to drop down (e.g. is there a railing in the way or some other bs!)
 * (called from BuildDropDownEdges)
 * @param SourcePoly - source (where we're coming from)
 * @param DestPoly - dest (which poly we trying to jump to)
 * @param SrcPos - source position to test
 * @param DestPos - destination position to test
 */
UBOOL IsDropDownTrajectoryClear(FNavMeshPolyBase* SourcePoly, FNavMeshPolyBase* DestPoly, const FVector& SrcPos, const FVector& DestPos)
{
	// Adjust drop down destination for recast generated polys, to compensate for voxel grid
	// (in that case NavMesh is not aligned perfectly to collision geometry but to grid
	//  and offset between edge of NavMesh and edge of geometry cause trace to fail)
	FVector AdjDestPos = DestPos;
	if (DestPoly->NavMesh->GetPylon()->NavMeshGenerator == NavMeshGenerator_Recast)
	{
		// Moving trajectory test points towards (and sligtly beyond) geometry edge
		// allows to get trace under the same conditions as in UE generated mesh
		AdjDestPos += (DestPos - SrcPos).SafeNormal2D() * 20.0f;
	}

	FVector SrcTestPos = SrcPos;
	SourcePoly->AdjustPositionToDesiredHeightAbovePoly(SrcTestPos,NAVMESHGEN_ENTITY_HALF_HEIGHT);
	FVector MidPt = AdjDestPos;
	SourcePoly->AdjustPositionToDesiredHeightAbovePoly(MidPt,NAVMESHGEN_ENTITY_HALF_HEIGHT);
	FVector DestTestPos = AdjDestPos;
	DestPoly->AdjustPositionToDesiredHeightAbovePoly(DestTestPos,NAVMESHGEN_ENTITY_HALF_HEIGHT);

	FCheckResult NewHit(1.0f);
	AScout* Scout = FPathBuilder::GetScout();
	FVector Extent = FVector(NAVMESHGEN_STEP_SIZE*0.5f);
	if (GWorld->SingleLineCheck(NewHit,Scout,MidPt,SrcTestPos, EXPANSION_TRACE_FLAGS,Extent))
	{
		if (GWorld->SingleLineCheck(NewHit,Scout,DestTestPos,MidPt, EXPANSION_TRACE_FLAGS,Extent))
		{
			return TRUE;
		}
	}

	return FALSE;
}

 /**
  * looks for a dropedgemesh on this navmesh and will attempt to 
  * create the proper dropdown edges based on the drop edge mesh
  */
extern void AddBorderEdgeSegmentsForPoly(FNavMeshPolyBase* Poly, TArray<UNavigationMeshBase::BorderEdgeInfo>& BorderEdgeSegments);
void UNavigationMeshBase::BuildDropDownEdges(TArray<FEdgeRef>& out_AddedEdges)
{

	static UBOOL bSkipDropDownEdges = FALSE;

	if ( bSkipDropDownEdges )
	{
		return;
	}

	// merge together dropdown mesh polys so we get continuous polys representing dropdown-able space
	MergeDropDownMesh();


	TArray<UNavigationMeshBase*> DropMeshes;
	// build a list of dropdown meshes that we should check against
	if( DropEdgeMesh != NULL)
	{
		DropMeshes.AddItem(DropEdgeMesh);
	}
	
	TArray<APylon*> PylonsThatOverlap;
	FVector Ctr(0.f), Extent(0.f);
	GetPylon()->GetBounds(WORLD_SPACE).GetCenterAndExtents(Ctr,Extent);
	UNavigationHandle::GetAllOverlappingPylonsFromBox(Ctr,Extent,PylonsThatOverlap);

	UNavigationMeshBase* TestMesh = NULL;
	for( INT PylonIdx=0;PylonIdx<PylonsThatOverlap.Num();++PylonIdx)
	{
		if( PylonsThatOverlap(PylonIdx) == GetPylon())
		{
			continue;
		}

		TestMesh = PylonsThatOverlap(PylonIdx)->NavMeshPtr;
		
		if( TestMesh != NULL && TestMesh->DropEdgeMesh != NULL )
		{
			DropMeshes.AddItem(TestMesh->DropEdgeMesh);
		}
	}

	// foreach dropmesh foreach droppoly
	//  -> find all polys in this mesh which overlap the drop poly
	//  -> also find all polys in the drop poly's owning mesh which overlap
	//  --> foreach poly in the list find border segments of it, then find intersections between that border segment and other polys in the list 
	
	// polys that intersect the current dropdown poly
	TArray<FNavMeshPolyBase*> Polys;

	// the current drop down mesh we're working with
	UNavigationMeshBase* CurrentDropMesh = NULL;
	for(INT DropMeshIdx=0;DropMeshIdx<DropMeshes.Num();++DropMeshIdx)
	{
		CurrentDropMesh = DropMeshes(DropMeshIdx);
		UNavigationMeshBase* OtherNavMesh = Cast<UNavigationMeshBase>(CurrentDropMesh->GetOuter());

		for( PolyList::TIterator It = CurrentDropMesh->BuildPolys.GetHead();It;++It)
		{
			Polys.Reset();

			FNavMeshPolyBase* CurDropPoly = *It;
			TArray<FVector> Locs;
			for(INT Idx=0;Idx<CurDropPoly->PolyVerts.Num();++Idx)
			{
				Locs.AddItem(CurDropPoly->GetVertLocation(Idx,WORLD_SPACE));
			}

			// find a list of polys in this mesh that intersect the current drop poly
			GetIntersectingPolys(Locs,Polys,WORLD_SPACE);
			// and all the polys in the other mesh which intersect this drop poly
			//OtherNavMesh->GetIntersectingPolys(Locs,Polys,WORLD_SPACE);
			APylon* OverlappingPylon = NULL;
			for(INT OverlappingPylonIdx=0;OverlappingPylonIdx<PylonsThatOverlap.Num();++OverlappingPylonIdx)
			{
				OverlappingPylon = PylonsThatOverlap(OverlappingPylonIdx);
				if (OverlappingPylon == GetPylon() || OverlappingPylon->NavMeshPtr == NULL)
				{
					continue;
				}
				OverlappingPylon->NavMeshPtr->GetIntersectingPolys(Locs,Polys,WORLD_SPACE);
			}

			// now iterate over combined poly list:
			//  find border segments of each poly, try to link those border edges to other polys in the list
			for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
			{
				FNavMeshPolyBase* CurPoly = Polys(PolyIdx);
				
				// MT->dropdown edges not supported on imported meshes, as indices change after the dropdown edge is linked up
				if( CurPoly->NavMesh->GetPylon()->bImportedMesh )
				{
					continue;
				}

				TArray<BorderEdgeInfo> BorderEdgesForPoly;
				AddBorderEdgeSegmentsForPoly(CurPoly,BorderEdgesForPoly);
				// **V foreach border edge segment, find intersections with other polys in the list and try to add edges for them
				FVector V0(0.f),V1(0.f);
				FVector EntryPt(0.f),ExitPt(0.f);
				for(INT BorderEdgeIdx=0;BorderEdgeIdx<BorderEdgesForPoly.Num();++BorderEdgeIdx)
				{
					V0 = CurPoly->NavMesh->GetVertLocation(BorderEdgesForPoly(BorderEdgeIdx).Vert0);
					V1 = CurPoly->NavMesh->GetVertLocation(BorderEdgesForPoly(BorderEdgeIdx).Vert1);

					// find other polys which intersect this border segment!
					for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
					{
						FNavMeshPolyBase* DestTestPoly = Polys(PolyIdx);
						if(DestTestPoly == CurPoly)
						{
							continue;
						}

						// MT->dropdown edges not supported on imported meshes, as indices change after the dropdown edge is linked up
						if( DestTestPoly->NavMesh->GetPylon()->bImportedMesh )
						{
							continue;
						}


						
						FLOAT Dist=0.f;
						const FVector Delta = V1 - V0;
						const FLOAT Len = Delta.Size();
						const FVector DeltaDir = Delta/Len;

						// direction from edge to do tests in (e.g. start from edge step out toward dest poly to find if dest point is in dest poly)
						FVector TestDir = (Delta ^ FVector(0.f,0.f,1.f)).SafeNormal();
						if( (TestDir | (DestTestPoly->GetPolyCenter() - CurPoly->GetPolyCenter()).SafeNormal() ) < 0.f )
						{
							TestDir *= -1.0f;
						}
						const FVector TestOffset = TestDir * NAVMESHGEN_STEP_SIZE;

						FVector CurrentTestPt_BasePt = V0;
						FVector EdgeStartPoint = V0;
						FVector EdgeEndPoint = V0;
						UBOOL bLastPtWasInside=FALSE;
						UBOOL bCurrentInDestPoly = FALSE;
						UBOOL bCorrectHeight = FALSE;
						UBOOL bTrajectoryClear = FALSE;
	
						FVector HeightAdjustedPos(0.f);
#define SWEEP_STEP_SIZE 5.0f
						// go a bit extra so we're sure to get the last check in 
						FLOAT TestLen = Len + SWEEP_STEP_SIZE*2.0f;
						while(Dist < TestLen)
						{

							if( Dist > Len )
							{
								bCurrentInDestPoly = FALSE;
							}
							else
							{
								HeightAdjustedPos = CurrentTestPt_BasePt+TestOffset;
								DestTestPoly->AdjustPositionToDesiredHeightAbovePoly( HeightAdjustedPos, 0.f );
								bTrajectoryClear = IsDropDownTrajectoryClear(CurPoly,DestTestPoly,CurrentTestPt_BasePt, HeightAdjustedPos);
								bCorrectHeight = (HeightAdjustedPos.Z - CurrentTestPt_BasePt.Z) < -NAVMESHGEN_MAX_STEP_HEIGHT;

								//////////////////////////////////////////////////////////////////////////
								// debug
// 								if( !bTrajectoryClear )
// 								{
// 									GWorld->GetWorldInfo()->DrawDebugLine(CurrentTestPt_BasePt,HeightAdjustedPos,255,0,0,TRUE);
// 								}
// 
// 								if( !bCorrectHeight )
// 								{
// 									GWorld->GetWorldInfo()->DrawDebugLine(CurrentTestPt_BasePt,HeightAdjustedPos,255,255,0,TRUE);
//								}
								//////////////////////////////////////////////////////////////////////////

								bCurrentInDestPoly = bCorrectHeight && bTrajectoryClear && DestTestPoly->ContainsPoint(CurrentTestPt_BasePt + TestOffset,WORLD_SPACE);
							}
							
							// if we're in the dest poly move up the end point
							if( bCurrentInDestPoly )
							{
								EdgeEndPoint = CurrentTestPt_BasePt;

								// and if this is the first time we're in the dest poly save the start point
								if( !bLastPtWasInside )
								{
									EdgeStartPoint = CurrentTestPt_BasePt;
								}
							}

							// if we just left the dest poly, add the segment!
							if( bLastPtWasInside && !bCurrentInDestPoly )
							{
								if( (EdgeEndPoint - EdgeStartPoint).Size() >= NAVMESHGEN_MIN_EDGE_LENGTH )
								{
									AddDropDownEdge(CurPoly,DestTestPoly,EdgeStartPoint,EdgeEndPoint,out_AddedEdges);
								}
							}

						
							bLastPtWasInside = bCurrentInDestPoly;
							CurrentTestPt_BasePt += DeltaDir * SWEEP_STEP_SIZE;
							Dist += SWEEP_STEP_SIZE;
						}
					}// inner poly loop
				}// border edge segments loop
			}
		}
	}

}

void APylon::GetIntersectingPolys(const FVector& Loc,
									const FVector& Extent, 
									TArray<FNavMeshPolyBase*>& out_Polys, 
									UBOOL bIgnoreDynamic, 
									UBOOL bReturnBothDynamicAndStatic,
									DWORD TraceFlags/*=0*/)
{
	if( NavMeshPtr == NULL )
	{
		return;
	}
	checkSlowish(NavMeshPtr != NULL);
	NavMeshPtr->GetIntersectingPolys(Loc,Extent,out_Polys,WORLD_SPACE,bIgnoreDynamic,bReturnBothDynamicAndStatic,TraceFlags);
}

/**
 * helper function for GetIntersectingPolys which will call into the submesh if need be before adding to output polys list
 */
void ConditionalAddToOutputPolys(TArray<UNavigationMeshBase*>& out_Submeshes, FNavMeshPolyBase* Poly, TArray<FNavMeshPolyBase*>& out_Polys, UBOOL bIgnoreDynamic, UBOOL bReturnBothDynamicAndStatic)
{
	// if this poly has a submesh, return all those polys rather than this one poly
	if(!bIgnoreDynamic && Poly->NumObstaclesAffectingThisPoly > 0)
	{
		out_Submeshes.AddUniqueItem(Poly->GetSubMesh());
	}

	if(Poly->NumObstaclesAffectingThisPoly <= 0 || bReturnBothDynamicAndStatic)
	{
		out_Polys.AddUniqueItem(Poly);
	}
}

/** 
 * GetIntersectingPolys (to passed extent)
 * - returns a list of polys intersecting the passed extent
 * @param Loc - location of center of extent
 * @param Extent - extent of box to check
 * @param out_Polys - output array of intersecting polys
 * @param bWorldSpace - reference frame incoming params are in
 * @param bIgnoreDynamic - whether or not to ignore dynamic (sub) meshes (don't use this unless you know what you're doing.. :) Typically If there are dynamci polys around, they should be used over the static ones
 * @param bReturnBothDynamicAndStatic - if TRUE, BOTH dynamic and static polys will be returned.. using this is *DANGEROUS*! most of the time you should use dynamic polys if they exist
 *                                      as they are the 'correct' representation of the mesh at that point

 */
void UNavigationMeshBase::GetIntersectingPolys(const FVector& Loc,
												const FVector& Extent,
												TArray<FNavMeshPolyBase*>& out_Polys,
												UBOOL bWorldSpace/*=WORLD_SPACE*/,
												UBOOL bIgnoreDynamic/*=FALSE*/,
												UBOOL bReturnBothDynamicAndStatic/*=FALSE*/,
												UBOOL bRecursedForSubmesh/*=FALSE*/,
												DWORD TraceFlags/*=0*/)
{
	if( PolyOctree == NULL && ! KDOPInitialized)
		return;


	static TArray<UNavigationMeshBase*> Submeshes;

	if( ! bRecursedForSubmesh )
	{
		Submeshes.Reset();
	}

	if( KDOPInitialized )
	{
		FBox Box(0);
		FBox LS_Box(0);

		if(bWorldSpace)
		{
			Box = FBox::BuildAABB(Loc,FVector(Extent.X,Extent.Y,Max<FLOAT>(Extent.Z,NAVMESHGEN_MAX_POLY_HEIGHT)));
			LS_Box = FBox::BuildAABB(Loc,Extent);
			if (bNeedsTransform)
			{
				LS_Box = LS_Box.TransformBy(WorldToLocal);
			}
		}
		else
		{
			LS_Box = FBox::BuildAABB(Loc,Extent);
			Box = FBox::BuildAABB(Loc,FVector(Extent.X,Extent.Y,Max<FLOAT>(Extent.Z,NAVMESHGEN_MAX_POLY_HEIGHT)));
			if(bNeedsTransform)
			{
				Box = Box.TransformBy(WorldToLocal);
			}
		}
		
		static TArray<INT> TriIndices; 
		TriIndices.Reset();
		
		UNavigationMeshBase* ForNavMesh = this;
		
		if( IsObstacleMesh() )
		{
			APylon* Pylon = GetPylon();
			if( Pylon->ObstacleMesh == this )
			{
				ForNavMesh = Pylon->NavMeshPtr;
			}
			else
			{
				// if this is the dynamic navmesh, there should not be any edges linked to polys anyway so do this to tell shouldcheckmatieral to treat everything as collision
				ForNavMesh = NULL;
			}
		}

		FNavMeshCollisionDataProvider Prov(this,ForNavMesh,TraceFlags);

		TkDOPAABBQuery<FNavMeshCollisionDataProvider,WORD> kDOPQuery(Box,TriIndices,Prov,bNeedsTransform);

		KDOPTree.AABBQuery(kDOPQuery);
		static TArray<WORD> AlreadyChecked;
		AlreadyChecked.Empty(AlreadyChecked.Num());
		for(INT Idx=0;Idx<TriIndices.Num();++Idx)
		{
			WORD PolyID = KDOPTree.Triangles(TriIndices(Idx)).MaterialIndex;
			if( AlreadyChecked.FindItemIndex(PolyID) == INDEX_NONE )
			{
				FNavMeshPolyBase* CurPoly = GetPolyFromId(PolyID);
				AlreadyChecked.AddItem(PolyID);
				if( CurPoly && CurPoly->BoxBounds.Intersect(LS_Box) )
				{
					ConditionalAddToOutputPolys(Submeshes,CurPoly,out_Polys,bIgnoreDynamic,bReturnBothDynamicAndStatic);
				}
			}
		}

	}
	else
	{
		FBox LS_Box = FBox::BuildAABB(Loc,Extent);
		if (bWorldSpace && bNeedsTransform)
		{
			LS_Box = LS_Box.TransformBy(WorldToLocal);
		}

		FBoxCenterAndExtent QueryBox(LS_Box.GetCenter(),LS_Box.GetExtent());

		for(FPolyOctreeType::TConstElementBoxIterator<> OctreeIt(*(PolyOctree),QueryBox); OctreeIt.HasPendingElements(); OctreeIt.Advance())
		{
			FNavMeshPolyBase* Poly = OctreeIt.GetCurrentElement();
			ConditionalAddToOutputPolys(Submeshes,Poly,out_Polys,bIgnoreDynamic,bReturnBothDynamicAndStatic);
		}
	} 


	if( ! bRecursedForSubmesh )
	{
		for( INT SubMeshIdx=0;SubMeshIdx<Submeshes.Num();++SubMeshIdx)
		{
			UNavigationMeshBase* SubMesh = Submeshes(SubMeshIdx);

			if( SubMesh != NULL )
			{
				SubMesh ->GetIntersectingPolys(Loc,Extent,out_Polys,bWorldSpace,FALSE,FALSE,TRUE);
			}
		}
	}
}

void UNavigationMeshBase::GetIntersectingPolys(const TArray<VERTID>& PolyVerts, TArray<FNavMeshPolyBase*>& out_Polys, DWORD TraceFlags)
{
	FBox PolyBounds(0);
	static TArray<FVector> PolyVertLocs;
	PolyVertLocs.Reset();
	for(INT Idx=0;Idx<PolyVerts.Num();Idx++)
	{
		PolyVertLocs.AddItem(GetVertLocation(PolyVerts(Idx),LOCAL_SPACE));
	}

	GetIntersectingPolys(PolyVertLocs,out_Polys,LOCAL_SPACE,TraceFlags);
}

/** 
* GetIntersectingPolys (to passed poly made up of vertids)
* returns a list of polys intersecting the passed poly
* @param PolyVerts - verts of poly to check intersections for
* @param out_Polys - output array of intersecting polys
* @param bWorldSpace - reference frame of incoming vert lcoations
*/	
void UNavigationMeshBase::GetIntersectingPolys(const TArray<FVector>& PolyVerts, TArray<FNavMeshPolyBase*>& out_Polys, UBOOL bWorldSpace, DWORD TraceFlags/*=0*/)
{
	FBox PolyBounds(0);
	static TArray<FVector> PolyVertLocs;
	PolyVertLocs.Reset();

	for(INT Idx=0;Idx<PolyVerts.Num();Idx++)
	{
		FVector VertLoc(0.f);
		if(bWorldSpace)
		{
			VertLoc =  W2LTransformFVector(PolyVerts(Idx));
		}
		else
		{
			VertLoc = PolyVerts(Idx);
		}

		PolyBounds += VertLoc;
		PolyBounds += VertLoc+FVector(0.f,0.f,+NAVMESHGEN_ENTITY_HALF_HEIGHT);
		PolyBounds += VertLoc+FVector(0.f,0.f,5.f);
		PolyVertLocs.AddItem(VertLoc);
	}

	static TArray<FNavMeshPolyBase*> PolysThatIsect;
	PolysThatIsect.Reset();
	//(const FVector& Loc, const FVector& Extent, TArray<FNavMeshPolyBase*>& out_Polys, UBOOL bWorldSpace, UBOOL bIgnoreDynamic/*=FALSE*/, UBOOL bReturnBothDynamicAndStatic/*=FALSE*/)
	GetIntersectingPolys(PolyBounds.GetCenter(), PolyBounds.GetExtent(),PolysThatIsect,LOCAL_SPACE,TRUE, TraceFlags);
	for(INT PolyIdx=0;PolyIdx<PolysThatIsect.Num();++PolyIdx)
	{
		FNavMeshPolyBase* CurPoly = PolysThatIsect(PolyIdx);
		if(CurPoly->IntersectsPoly(PolyVertLocs,LOCAL_SPACE))
		{ 
				out_Polys.AddItem(CurPoly);
		}
	}
}

void APylon::PostEditMove(UBOOL bFinished)
{
	Super::PostEditMove(bFinished);

	// do not allow negative scale
	DrawScale3D.X = Abs<FLOAT>(DrawScale3D.X);
	DrawScale3D.Y = Abs<FLOAT>(DrawScale3D.Y);
	DrawScale3D.Z = Abs<FLOAT>(DrawScale3D.Z);
	DrawScale = Abs<FLOAT>(DrawScale);		


	FLOAT MaxVolume = MaxExpansionRadius*MaxExpansionRadius*MaxExpansionRadius;
	FVector NewScale = DrawScale3D * DrawScale * ExpansionRadius;
	FLOAT NewVolume = NewScale.X*NewScale.Y*NewScale.Z;


	if(NewVolume > MaxVolume)
	{
		// find a scalar that will bring the volume in check 
		FLOAT XScale = appPow(MaxVolume / (DrawScale3D.X * DrawScale3D.Y * DrawScale3D.Z),0.33) / (ExpansionRadius*DrawScale);
		DrawScale3D *= XScale;
		ForceUpdateComponents();
	}

	if( GIsEditor && PylonRadiusPreview != NULL)
	{
		PylonRadiusPreview->BeginDeferredReattach();
	}
}

void APylon::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	// do not allow negative scale
	DrawScale3D.X = Abs<FLOAT>(DrawScale3D.X);
	DrawScale3D.Y = Abs<FLOAT>(DrawScale3D.Y);
	DrawScale3D.Z = Abs<FLOAT>(DrawScale3D.Z);
	DrawScale = Abs<FLOAT>(DrawScale);		


	FLOAT MaxVolume = MaxExpansionRadius*MaxExpansionRadius*MaxExpansionRadius;
	FVector NewScale = DrawScale3D * DrawScale * ExpansionRadius;
	FLOAT NewVolume = NewScale.X*NewScale.Y*NewScale.Z;

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged != NULL && PropertyThatChanged->GetFName() == FName(TEXT("ExpansionRadius")) )
	{
		if(NewVolume > MaxVolume)
		{
			ExpansionRadius = appPow(MaxVolume / (DrawScale3D.X * DrawScale3D.Y * DrawScale3D.Z),0.33) / DrawScale;
		}
		ForceUpdateComponents();
	}

	if( PropertyThatChanged != NULL && PropertyThatChanged->GetFName() == FName(TEXT("DrawScale")) )
	{
		if(NewVolume > MaxVolume)
		{
			DrawScale = appPow(MaxVolume / (DrawScale3D.X * DrawScale3D.Y * DrawScale3D.Z),0.33) / ExpansionRadius;
		}
		ForceUpdateComponents();
	}

	if( PropertyThatChanged != NULL && PropertyThatChanged->GetFName() == FName(TEXT("DrawScale3D")) )
	{

		if(NewVolume > MaxVolume)
		{
			// find a scalar that will bring the volume in check 
			FLOAT XScale = appPow(MaxVolume / (DrawScale3D.X * DrawScale3D.Y * DrawScale3D.Z),0.33) / (ExpansionRadius*DrawScale);
			DrawScale3D *= XScale;
			ForceUpdateComponents();
		}

		ForceUpdateComponents();
	}

	//debug
	if( PropertyThatChanged != NULL && PropertyThatChanged->GetFName() == FName(TEXT("DebugEdgeCount")) )
	{
		if( NavMeshPtr != NULL )
		{
			NavMeshPtr->CreateEdgeConnections( TRUE );
		}
	}
}

// returns a bounding box for our expansion bounds
FBox APylon::GetExpansionBounds()
{
	FBox Bounds;
	Bounds.Init();

	UBOOL bAtLeastOneNonNull = FALSE;
	for(INT VolumeIdx=0;VolumeIdx<ExpansionVolumes.Num();VolumeIdx++)
	{
		AVolume* CurVol = ExpansionVolumes(VolumeIdx);
		if(CurVol != NULL && CurVol->Brush != NULL)
		{
			bAtLeastOneNonNull=TRUE;
			Bounds += CurVol->GetComponentsBoundingBox(TRUE);
		}
		
	}

	if(!bAtLeastOneNonNull)
	{
		FVector Scale = DrawScale3D * DrawScale * ExpansionRadius;
		Bounds = FBox::BuildAABB(GetExpansionSphereCenter(),Scale);
	}

	return Bounds;
}

FBox APylon::GetBounds(UBOOL bWorldSpace)
{
	if(!GIsGame && !GIsPlayInEditorWorld && !bImportedMesh)
	{
		FBox Bounds = (bWorldSpace) ? GetExpansionBounds() : GetExpansionBounds().TransformBy(GetMeshWorldToLocal());
		return Bounds.ExpandBy(NAVMESHGEN_STEP_SIZE*2.0f);
	}

	if(NavMeshPtr != NULL)
	{
		return (bWorldSpace && NavMeshPtr->bNeedsTransform) ? NavMeshPtr->BoxBounds.TransformBy(NavMeshPtr->LocalToWorld) : NavMeshPtr->BoxBounds;
	}

	return FBox(0);
}


// returns FALSE if the passed point is not within our expansion constraints
UBOOL   APylon::IsPtWithinExpansionBounds(const FVector& Pt, FLOAT Buffer)
{
	// allow pathobjects to restrict expansion
	if(PathObjectsThatAffectThisPylon != NULL && PathObjectsThatAffectThisPylon->Num() > 0)
	{
		for(INT PathObjectIdx=0;PathObjectIdx<PathObjectsThatAffectThisPylon->Num();++PathObjectIdx)
		{
			IInterface_NavMeshPathObject* CurrentPO = (*PathObjectsThatAffectThisPylon)(PathObjectIdx);
			if(!CurrentPO->IsExplorationAllowed(this,Pt))
			{
				return FALSE;
			}
		}
	}

	// we have some volumes, check them each 
	UBOOL bAtLeastOneNonNull = FALSE;
	for(INT VolumeIdx=0;VolumeIdx<ExpansionVolumes.Num();VolumeIdx++)
	{
		AVolume* CurVol = ExpansionVolumes(VolumeIdx);
		if(CurVol != NULL)
		{
			bAtLeastOneNonNull=TRUE;
			if(CurVol->Encompasses(Pt,FVector((Buffer>0.f) ? Buffer : 0.f)))
			{
				return TRUE;
			}
		}
	}

	// if we got here, and there is at least one valid volume, then we're good
	if(bAtLeastOneNonNull)
	{
		return FALSE;
	}

	//return 	(GetExpansionSphereCenter() - Pt).SizeSquared() < ((ExpansionRadius+Buffer) * (ExpansionRadius+Buffer));

	// no volumes, just use radius (AABB)
	FVector Scale = DrawScale3D * DrawScale * (ExpansionRadius+Buffer);
	FBox DaBox = FBox::BuildAABB(GetExpansionSphereCenter(),Scale);
	return DaBox.IsInside(Pt);
	
}

void UNavigationMeshBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if( Ar.IsObjectReferenceCollector() )
	{
 		
		// add refs to pylons ref'd by dynamic edges
		for(DynamicEdgeList::TIterator Itt(DynamicEdges);Itt;++Itt)
 		{
 			FNavMeshEdgeBase* CurEdge = Itt.Value();
 			CurEdge->Serialize(Ar);
 		}

		// add refs to pylons ref'd by cross-pylon edges
		for(INT CPIdx=0;CPIdx<CrossPylonEdges.Num();CPIdx++)
		{
			FNavMeshCrossPylonEdge* CPEdge = CrossPylonEdges(CPIdx);
			
			// if it's reffing an actor in the same level, emit a reference
			if(CPEdge->Poly0Ref.OwningPylon.Actor != NULL && CPEdge->Poly1Ref.OwningPylon.Actor != NULL && 
				CPEdge->Poly0Ref.OwningPylon.Actor->GetOutermost() == CPEdge->Poly1Ref.OwningPylon.Actor->GetOutermost())
			{
				Ar << CPEdge->Poly0Ref.OwningPylon.Actor;
				Ar << CPEdge->Poly1Ref.OwningPylon.Actor;
			}
		}

		// add references to cover ref'd by polys
		for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
		{
			FNavMeshPolyBase* Poly = &Polys(PolyIdx);
			for(INT PolyCoverIdx=0;PolyCoverIdx<Poly->PolyCover.Num();++PolyCoverIdx)
			{
				if( Poly->PolyCover(PolyCoverIdx).Actor != NULL &&
					Poly->PolyCover(PolyCoverIdx).Actor->GetOutermost() == GetOutermost())
				{
					Ar << Poly->PolyCover(PolyCoverIdx).Actor;
				}
			}
		}

		// add refs to submeshes 
		for( PolyObstacleInfoList::TIterator It(PolyObstacleInfoMap);It;++It)
		{
			FPolyObstacleInfo& Info = It.Value();
			if(Info.SubMesh != NULL)
			{
				Ar << Info.SubMesh;
			}
		}

		// ref dropedgemesh
		Ar << DropEdgeMesh;

	}

	if( Ar.IsCountingMemory() )
	{
		Ar << Verts;
		Ar << Polys;
		Ar << EdgeDataBuffer;
		Ar << EdgeStorageData;
		Ar << KDOPTree;
		SubMeshToParentPolyMap.CountBytes(Ar);
		BorderEdgeSegments.CountBytes(Ar);
		Polys.CountBytes(Ar);
		CrossPylonEdges.CountBytes(Ar);
		EdgePtrs.CountBytes(Ar);
	}

	if( Ar.IsSaving() || Ar.IsLoading() )
	{
		Ar << NavMeshVersionNum;

		if(NavMeshVersionNum >= VER_PATHDATA_VERSION)
		{
			Ar << VersionAtGenerationTime;
			if ( Ar.IsLoading() )
			{
				FPathBuilder::SetPathingVersionNum(VersionAtGenerationTime);
			}
		}

		Ar << Verts;
		Ar << EdgeStorageData;
		Ar << Polys;

		// if this is an older version than VER_REMOVED_BASEACTOR we need to pretend to care about the old 'BaseActor' ref
		if(NavMeshVersionNum < VER_REMOVED_BASEACTOR)
		{
			AActor* DummyBaseActor=NULL;
			Ar << DummyBaseActor;
		}

		if(NavMeshVersionNum >= VER_SERIALIZE_TRANSFORMS)
		{
			Ar << LocalToWorld;
			Ar << WorldToLocal;
		}

		if(NavMeshVersionNum >= VER_SERIALIZE_BORDEREDGELIST)
		{
			if(NavMeshVersionNum >= VER_EDGE_WIDTH_GENERATION || (GetPylon() && !GetPylon()->IsStatic())) //@FIXME: dangerous to base serialization on script property values
			{
				Ar << BorderEdgeSegments;
			}			
		}
		

		// if we just loaded, construct our edges
		if(Ar.IsLoading())
		{
			ConstructLoadedEdges();
		}

		if(NavMeshVersionNum >= VER_MESH_BOUNDS)
		{
			Ar << BoxBounds;
		}
		else
		{
			BuildBounds();
		}
	}

	// always serialize edges 
	for(INT EdgeIdx=0;EdgeIdx<GetNumEdges();EdgeIdx++)
	{
		FNavMeshEdgeBase* Edge = GetEdgeAtIdx(EdgeIdx);
		Edge->Serialize(Ar);
	}

}

void UNavigationMeshBase::BuildBounds()
{
	BoxBounds = FBox(0);
	for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
	{
		BoxBounds += Polys(PolyIdx).BoxBounds;
	}
}


struct FEdgeInfoPair
{
	FEdgeInfoPair(FVector InPt0, FVector InPt1, FVector InEdgeCtr) : Pt0(InPt0), Pt1(InPt1), EdgeCtr(InEdgeCtr) {}
	
	FVector Pt0;
	FVector Pt1;
	FVector EdgeCtr;

};
typedef TDoubleLinkedList<FEdgeInfoPair> SortedEdgeList;
UBOOL PointsEqualEnough(const FVector& PtA, const FVector& PtB, FLOAT Tolerance)
{
	return ( appIsNearlyEqual(PtA.X,PtB.X,Tolerance) && appIsNearlyEqual(PtA.Y,PtB.Y,Tolerance) && appIsNearlyEqual(PtA.Z,PtB.Z,NAVMESHGEN_MAX_STEP_HEIGHT) );
}

void InsertSortedEdgeForSeg(const FVector& NewEdgePt0, const FVector& NewEdgePt1, const FVector& Pt0OfSeg, SortedEdgeList& EdgeList, UNavigationMeshBase* Mesh)
{

	const FVector Delta = (NewEdgePt1-NewEdgePt0);
	// skip zero length edges
	if(Delta.Size() < NAVMESHGEN_OBSTACLEMESH_GAP_EPSILON)
	{
		return;
	}

	const FVector NewEdgeCtr = NewEdgePt0 + (Delta*0.5f);

	FLOAT Dist = (NewEdgeCtr - Pt0OfSeg).SizeSquared();
	// insert sort based on distance of center of new edge to poly0
	for(SortedEdgeList::TIterator Itt(EdgeList.GetHead());Itt;++Itt)
	{
		FEdgeInfoPair& CurEdge = (*Itt);
		FLOAT CurDist = (CurEdge.EdgeCtr - Pt0OfSeg).SizeSquared();

static FLOAT Tolerance = 5.0f;
#define TOL Tolerance
		
		if( ( PointsEqualEnough(CurEdge.Pt0,NewEdgePt0,TOL) && PointsEqualEnough(CurEdge.Pt1,NewEdgePt1,TOL) ) ||
			( PointsEqualEnough(CurEdge.Pt0,NewEdgePt1,TOL) && PointsEqualEnough(CurEdge.Pt1,NewEdgePt0,TOL) ) )
		{
			// this edge is already in the list.. skip it
			return;
		}

		if(CurDist >= Dist)
		{
			EdgeList.InsertNode(FEdgeInfoPair(NewEdgePt0,NewEdgePt1, NewEdgeCtr),Itt.GetNode());
			return;
		}
	}

	// otherwise add at the end
	EdgeList.AddTail(FEdgeInfoPair(NewEdgePt0,NewEdgePt1, NewEdgeCtr));
}

/**
 * given a poly edge span and a list of temporary edge data that represent adjacent polys
 * create obstacle geo where appropriate
 * NOTE: Expected locations should be in local space
 * INTERNAL function called by the BuildObstacleMesh functions
 * @param NormalMesh - the mesh we're building an obstacle mesh for (e.g. the navmesh itself)
 * @param ObsMesh - the obstacle mesh where we're putting the data
 * @param SegmentEndPoint0 - the starting point of the segment span
 * @param SegmentEndPoint1 - the ending point for the segment span
 * @param Vert0ID - ID of the first vertex for segment span
 * @param Vert1ID - ID of the second vertex for segment span
 * @param SegmentEdges - the list of edges that represents adjacent polys
 * @param HeightOffsetVec - vector used for determining the offset off the base mesh to add the obstacle geometry
 * @param CurPOly - the current navigable poly we're adding geo for
 * @param OutPutPolys - optional list to stuff with info about polys we're adding
 * @return TRUE if any geo was added
 */
#define GAP_EPSILON NAVMESHGEN_OBSTACLEMESH_GAP_EPSILON
UBOOL AddObstacleGeoForPolySegment(UNavigationMeshBase* NormalMesh,
								   UNavigationMeshBase* ObstacleMesh,
								   const FVector& SegmentEndPoint0,
								   const FVector& SegmentEndPoint1,
								   VERTID Vert0ID,
								   VERTID Vert1ID,
								   SortedEdgeList& SegmentEdges,
								   FVector HeightOffsetVec, 
								   FNavMeshPolyBase* CurPoly,
								   PolyList* OutPutPolys=NULL,
								   UBOOL bWorldSpace=WORLD_SPACE)
{
	// keep track of whether this edge segment had any obstacles on it, if so mark it as a border edge
	UBOOL bSegmentContainsObstacleGeo = FALSE;

	// sweep along the segment, and find the gaps not filled by Edges (cuz that's our border geo!)
	FVector CurrentEventPoint = SegmentEndPoint0;
	for(SortedEdgeList::TIterator Itt(SegmentEdges.GetHead());Itt;++Itt)
	{
		FEdgeInfoPair& CurEdgeInfo = (*Itt);

		FVector NewEventPoint = CurEdgeInfo.Pt0;

		INT VertUsed=0;
		FLOAT NewEventDist = (NewEventPoint - CurrentEventPoint).Size2D();
		FLOAT Vert1Dist = (CurEdgeInfo.Pt1 - CurrentEventPoint).Size2D();
		if(  Vert1Dist < NewEventDist )
		{
			VertUsed=1;
			NewEventPoint = CurEdgeInfo.Pt1;
			NewEventDist = Vert1Dist;
		}

		// if the new event point is further than epsilon away from the last one, add a poly to the obstacle mesh along the gap
		// NOTE: if this is on a cross pylon edge, add border geo for it, and mark the new geo with the CP edge
		if(NewEventDist > GAP_EPSILON)
		{
			FVector BottomCorner0 = CurrentEventPoint;
			FVector BottomCorner1 = NewEventPoint;

			static TArray<FVector> Verts;
			Verts.Reset();
			Verts.AddItem(BottomCorner1+HeightOffsetVec);
			Verts.AddItem(BottomCorner1);
			Verts.AddItem(BottomCorner0);
			checkSlowish(!(BottomCorner0 - BottomCorner1).IsNearlyZero(0.1f));
			Verts.AddItem(BottomCorner0+HeightOffsetVec);
			FNavMeshPolyBase* NewPoly = ObstacleMesh->AddPoly(Verts,200.f,bWorldSpace);

			if (NewPoly!=NULL)
			{
				bSegmentContainsObstacleGeo = TRUE;
			}

			if( OutPutPolys != NULL && NewPoly != NULL )
			{
				OutPutPolys->AddHead(NewPoly);
			}

					//GWorld->GetWorldInfo()->DrawDebugLine(NewPoly->CalcCenter(),CurPoly.CalcCenter(),255,255,255,TRUE);
		}

		// set the current event point to the other vert in the current edge (skip over the part the edge covers) 
		CurrentEventPoint = (VertUsed==0) ? CurEdgeInfo.Pt1 : CurEdgeInfo.Pt0;
	}

	// check for a gap at the end
	FLOAT EndGap = (CurrentEventPoint - SegmentEndPoint1 ).Size2D();
	if(EndGap > GAP_EPSILON)
	{
		FVector BottomCorner0 = CurrentEventPoint;
		FVector BottomCorner1 = SegmentEndPoint1;

		static TArray<FVector> Verts;
		Verts.Reset();
		Verts.AddItem(BottomCorner1+HeightOffsetVec);
		Verts.AddItem(BottomCorner1);
		Verts.AddItem(BottomCorner0);
		Verts.AddItem(BottomCorner0+HeightOffsetVec);
		checkSlowish(!(BottomCorner0 - BottomCorner1).IsNearlyZero(0.1f));

		FNavMeshPolyBase* NewPoly = ObstacleMesh->AddPoly(Verts,200.f,bWorldSpace);

		if (NewPoly!=NULL)
		{
			bSegmentContainsObstacleGeo = TRUE;
		}
		if( ( OutPutPolys != NULL ) && ( NewPoly != NULL ) )
		{
			OutPutPolys->AddHead(NewPoly);
		}
		//GWorld->GetWorldInfo()->DrawDebugLine(NewPoly->CalcCenter(),CurPoly.CalcCenter(),255,255,255,TRUE);
	}

	if( bSegmentContainsObstacleGeo && CurPoly != NULL)
	{
		NormalMesh->BorderEdgeSegments.AddItem(UNavigationMeshBase::BorderEdgeInfo(Vert0ID,Vert1ID,CurPoly->Item));
	}

	return bSegmentContainsObstacleGeo;
}

/**
 * ***NOTE: this version is called during submesh builds.. and relies on edges being in place.
 * this function will loop through all boundary segments on 'NormalMesh' and create obstacle mesh geometry for pieces of the mesh that are 
 * not connected to the rest of the mesh, forming obstacle boundaries
 * @param NormalMesh - the mesh to create obstacle geo from
 * @param ObstacleMesh - the output mesh to add obstacle geo to
 * @param bSecondPass - second pass is when polys for special edges are added, first pass is boundary shapes only
 * @param bSkipSimpliciation - when TRUE the new obstacle mesh will not be simplified (e.g. at runtime)
 * @param OutputPolys (optional) - if specified, polys added to obstaclemesh will also be added to this list (so you can keep track of what was added this call, rather than what was already in the mesh)
 * @return TRUE if any geo was added to the obstacle mesh
 */
UBOOL BuildObstacleMeshForSubMesh(FPolyObstacleInfo* PolyObstacleInfo, UNavigationMeshBase* ObstacleMesh, UBOOL bSecondPass, UBOOL bSkipSimplification=FALSE, PolyList* OutPutPolys=NULL)
{

	UBOOL bAddedGeo=FALSE;
	FLOAT GapThresh = GetStepSize(Max<INT>(0,NAVMESHGEN_MAX_SUBDIVISIONS-1));
	UNavigationMeshBase* NormalMesh = PolyObstacleInfo->SubMesh;
#define GAP_EPSILON NAVMESHGEN_OBSTACLEMESH_GAP_EPSILON
	static TArray<FVector> Verts;

	if( NormalMesh == NULL )
	{
		return FALSE;
	}
	for(INT Idx=0;Idx<NormalMesh->Polys.Num();Idx++)
	{
		FNavMeshPolyBase& CurPoly = NormalMesh->Polys(Idx);
		const FVector OffsetVec = FVector(0.f,0.f,CurPoly.PolyHeight);

		const FVector PolyNorm = CurPoly.GetPolyNormal(LOCAL_SPACE);
		const FVector PolyCtr = CurPoly.GetPolyCenter(LOCAL_SPACE);

		if( bSecondPass )
		{

			// Add obstacle geo for edges that need it (e.g. one way edges, sepcial edges etc)
			for(INT EdgeIdx=0;EdgeIdx<CurPoly.GetNumEdges();EdgeIdx++)
			{
				WORD EdgePoolIdx = (EdgeIdx < CurPoly.PolyEdges.Num()) ? CurPoly.PolyEdges(EdgeIdx) : MAXWORD;
				FNavMeshEdgeBase* CurPolyEdge = CurPoly.GetEdgeFromIdx(EdgeIdx);

				if(CurPolyEdge != NULL && CurPolyEdge->NeedsObstacleGeoAddedDuringSubMeshBuilds() && CurPolyEdge->GetPoly0() == &CurPoly )
				{

					// if this edge wants there to be an obstacle mesh poly for it, add one now
					FVector BottomCorner0 = CurPolyEdge->GetVertLocation(0,LOCAL_SPACE);
					FVector BottomCorner1 = CurPolyEdge->GetVertLocation(1,LOCAL_SPACE);
					checkSlowish(!(BottomCorner0 - BottomCorner1).IsNearlyZero(0.1f));

					Verts.Reset();

					if( (((BottomCorner1 - BottomCorner0).SafeNormal() ^ PolyNorm) | (BottomCorner1-PolyCtr).SafeNormal()) < 0.f )
					{
						Verts.AddItem(BottomCorner1+OffsetVec);
						Verts.AddItem(BottomCorner1);
						Verts.AddItem(BottomCorner0);
						Verts.AddItem(BottomCorner0+OffsetVec);
					}
					else
					{
						Verts.AddItem(BottomCorner0+OffsetVec);
						Verts.AddItem(BottomCorner0);
						Verts.AddItem(BottomCorner1);
						Verts.AddItem(BottomCorner1+OffsetVec);	
					}


					FNavMeshPolyBase* NewPoly = ObstacleMesh->AddPoly(Verts,200.f,LOCAL_SPACE);

					if( NewPoly != NULL )
					{
						bAddedGeo=TRUE;
					}
					if( ( OutPutPolys != NULL ) && ( NewPoly != NULL ) )
					{
						OutPutPolys->AddHead(NewPoly);
					}

				}
			}
		}
		else 
		// add edges for obstacle boundary shapes
		if( !bSecondPass )
		{
			for( INT Idx=0;Idx<PolyObstacleInfo->LinkedObstacles.Num();++Idx)
			{
				IInterface_NavMeshPathObstacle* LinkedObstacle = PolyObstacleInfo->LinkedObstacles(Idx);

				if( !LinkedObstacle->PreserveInternalPolys() )
				{			
					for(INT ShapeIdx=0;ShapeIdx<LinkedObstacle->GetNumBoundingShapes();++ShapeIdx)
					{
						Verts.Reset();
						LinkedObstacle->GetBoundingShape(Verts,ShapeIdx);

						for(INT Vertidx=0; Vertidx< Verts.Num();++Vertidx)
						{
							const FVector CurPt = Verts(Vertidx);
							const FVector NextPt = Verts((Vertidx+1) % Verts.Num() );

							// MT-> could do segment intersect with the parent poly here but it will be more expensive.. try to get away with center point check!
							if( PolyObstacleInfo->Poly->ContainsPoint( (CurPt+NextPt)*0.5f, WORLD_SPACE) )
							{
								static TArray<FVector> AddVerts;
								AddVerts.Reset(4);
								AddVerts.AddItem(CurPt+OffsetVec);
								AddVerts.AddItem(CurPt);
								AddVerts.AddItem(NextPt);
								AddVerts.AddItem(NextPt+OffsetVec);	
								FNavMeshPolyBase* NewPoly = ObstacleMesh->AddPoly(AddVerts,200.f,WORLD_SPACE);
								if( NewPoly != NULL )
								{
									bAddedGeo=TRUE;
								}
								if( ( OutPutPolys != NULL ) && ( NewPoly != NULL ) )
								{
									OutPutPolys->AddHead(NewPoly);
								}
							}
						}
					}
				}
			}
		}
		
	}

// 	if(NAVMESHGEN_EXPANSION_DO_OBSTACLE_MESH_SIMPLIFICATION && !bSkipSimplification)
// 	{
// 		ObstacleMesh->MergePolys(FVector(1.0f),FALSE);
// 	}


	return bAddedGeo;
}


/**
 * ***NOTE: this version is called during OFF-LINE builds.. and does not rely on edges
 * this function will loop through all boundary segments on 'NormalMesh' and create obstacle mesh geometry for pieces of the mesh that are 
 * not connected to the rest of the mesh, forming obstacle boundaries
 * @param NormalMesh - the mesh to create obstacle geo from
 * @param ObstacleMesh - the output mesh to add obstacle geo to
 * @param bSkipSimpliciation - when TRUE the new obstacle mesh will not be simplified (e.g. at runtime)
 * @param OutputPolys (optional) - if specified, polys added to obstaclemesh will also be added to this list (so you can keep track of what was added this call, rather than what was already in the mesh)
 * @return TRUE if any geo was added to the obstacle mesh
 */
UBOOL BuildObstacleMesh(UNavigationMeshBase* NormalMesh, UNavigationMeshBase* ObstacleMesh, UBOOL bSkipSimplification=FALSE, PolyList* OutPutPolys=NULL)
{

	UBOOL bAddedGeo=FALSE;
	FLOAT GapThresh = GetStepSize(Max<INT>(0,NAVMESHGEN_MAX_SUBDIVISIONS-1));
	for(INT Idx=0;Idx<NormalMesh->Polys.Num();Idx++)
	{
		FNavMeshPolyBase& CurPoly = NormalMesh->Polys(Idx);

		const FVector OffsetVec = FVector(0.f,0.f,CurPoly.PolyHeight);
		// for each segment along the boundary of this polygon
		for(INT VertIdx=0;VertIdx<CurPoly.PolyVerts.Num();VertIdx++)
		{
			VERTID Vert0 = CurPoly.PolyVerts(VertIdx);
			VERTID Vert1 = CurPoly.PolyVerts( (VertIdx+1) % CurPoly.PolyVerts.Num() );
			FVector SegmentEndPoint0 = NormalMesh->GetVertLocation(Vert0);
			FVector SegmentEndPoint1 = NormalMesh->GetVertLocation(Vert1);

			SortedEdgeList SegmentEdges;


			// find all poly edge spans adjacent to this one

			static TArray<FNavMeshPolyBase*> Polys;
			Polys.Reset();


			FBox Box(1);
			Box+=SegmentEndPoint0;
			Box+=SegmentEndPoint1;
			Box = Box.ExpandBy(5.f);
			NormalMesh->GetIntersectingPolys(Box.GetCenter(),Box.GetExtent(),Polys,WORLD_SPACE);

			
			static TArray<FNavMeshEdgeBase> Temp_Edges;
			Temp_Edges.Reset();

			for( INT PolyIdx = 0; PolyIdx < Polys.Num(); ++PolyIdx )
			{
				NormalMesh->AddTempEdgesForPoly(*Polys(PolyIdx),Temp_Edges);
			}


			static TArray<UNavigationMeshBase::FEdgeTuple> AdjacentEdges;
			AdjacentEdges.Reset();

			NormalMesh->CreateEdgeConnectionsInternalFast(Temp_Edges,FALSE,FALSE,FALSE,NULL,&AdjacentEdges,-1.f,1.0f);
		
			for(INT TempEdgeIdx=0;TempEdgeIdx<AdjacentEdges.Num();++TempEdgeIdx)
			{
				
				FVector Inner_SegmentEndPoint0 = AdjacentEdges(TempEdgeIdx).Pt0;
				FVector Inner_SegmentEndPoint1 = AdjacentEdges(TempEdgeIdx).Pt1;

				FVector EdgeCtrNoZ = Inner_SegmentEndPoint0 + (Inner_SegmentEndPoint1-Inner_SegmentEndPoint0)*0.5f;
				FVector EdgeCtr = EdgeCtrNoZ;
				EdgeCtrNoZ.Z = 0.f;

				FVector ClosestSegPt0(0.f);					
				FVector ClosestSegPt1(0.f);					
				FLOAT T_0=0.f, T_1=0.f;
				FLOAT DistSq0 = SqPointDistToSegmentOutT(Inner_SegmentEndPoint0,SegmentEndPoint0,SegmentEndPoint1,ClosestSegPt0,T_0);
				FLOAT DistSq1 = SqPointDistToSegmentOutT(Inner_SegmentEndPoint1,SegmentEndPoint0,SegmentEndPoint1,ClosestSegPt1,T_1);
				FLOAT VertDist0 = Abs<FLOAT>(Inner_SegmentEndPoint0.Z - ClosestSegPt0.Z);
				FLOAT VertDist1 = Abs<FLOAT>(Inner_SegmentEndPoint1.Z - ClosestSegPt1.Z);

				if( DistSq0 < EDGE_VERT_TOLERANCE_SQ &&
					DistSq1 < EDGE_VERT_TOLERANCE_SQ &&
					VertDist0 < NAVMESHGEN_MAX_STEP_HEIGHT &&
					VertDist1 < NAVMESHGEN_MAX_STEP_HEIGHT)
				{
					InsertSortedEdgeForSeg(Inner_SegmentEndPoint0,Inner_SegmentEndPoint1,SegmentEndPoint0,SegmentEdges,NormalMesh);
				}
			}

		
			// add geo for this span and this list of edges
			// MT->EndPoints in world space!
			if ( AddObstacleGeoForPolySegment(NormalMesh,ObstacleMesh,SegmentEndPoint0,SegmentEndPoint1,Vert0, Vert1, SegmentEdges,OffsetVec,&CurPoly, OutPutPolys, WORLD_SPACE) )
			{
				bAddedGeo = TRUE;
			}
		}
		
	}

	return bAddedGeo;
}



/**
 * RebuildObstacleMeshGeometryForLinkedEdges
 * - will split obstacle mesh polys where necessary in order to 
 *   have obstacle mesh geo that lines up with edges linked to it
 * @param MeshThatOwnsTheEdges - the mesh that owns the edges linked to our polys (since this is an obstacle msh eh?)
 */
struct FEdgeEdgeIDPair
{
	FEdgeEdgeIDPair(WORD InEdgeId, FNavMeshEdgeBase* InEdge)
	{
		EdgeID=InEdgeId;
		Edge=InEdge;
	}
	WORD EdgeID;
	FNavMeshEdgeBase* Edge;
};

void UNavigationMeshBase::RebuildObstacleMeshGeometryForLinkedEdges(UNavigationMeshBase* MeshThatOwnsTheEdges)
{
	if( Polys.Num() > 0 )
	{
		BuildPolys.Clear();
		for( INT VertIdx = 0; VertIdx < Verts.Num(); ++VertIdx )
		{
			Verts(VertIdx).ContainingPolys.Empty();
		}

		// for each existing poly either copy it directly or add several new polys for it
		for( INT PolyIdx = 0; PolyIdx < Polys.Num(); ++PolyIdx )
		{			
			FNavMeshPolyBase& CurPoly = Polys(PolyIdx);
			if( Polys(PolyIdx).OctreeId.IsValidId() )
			{
				RemovePolyFromOctree(&Polys(PolyIdx));
			}
			
			if( CurPoly.PolyEdges.Num() == 0 )
			{
				AddPolyFromVertIndices(Polys(PolyIdx).PolyVerts,Polys(PolyIdx).PolyHeight);
			}
			else
			// ** if this obstacle poly has edges linked to it, split the poly up into other polys that line up with the edges linked.  so we only disable 
			//    the segments of the poly that are associated with the edges linked to it
			{
				// get edge points of poly segment
				check(CurPoly.PolyVerts.Num() == 4);

				// 0 should be top right, 1 should be bottom right, 2 should be bottom left, 3 should be top left.. we want 1 and 2 to stay in CW winding
				const FVector PolySegmentPt1 = CurPoly.GetVertLocation(1,LOCAL_SPACE);
				const FVector PolySegmentPt0 = CurPoly.GetVertLocation(2,LOCAL_SPACE);
				VERTID SegmentPt1 = CurPoly.PolyVerts(1);
				VERTID SegmentPt0 = CurPoly.PolyVerts(2);

				// build a list of edges we want to consider for this
				static TArray<FEdgeEdgeIDPair> EdgesToConsider;
				EdgesToConsider.Reset();
				for(INT EdgePolyIdx=0;EdgePolyIdx<CurPoly.PolyEdges.Num();++EdgePolyIdx)
				{
					WORD CurEdgeID = CurPoly.PolyEdges(EdgePolyIdx);
					FNavMeshEdgeBase* CurEdge = MeshThatOwnsTheEdges->GetEdgeAtIdx(CurEdgeID);
					
					
					FNavMeshPolyBase* Poly0 = CurEdge->GetPoly0();
					FNavMeshPolyBase* Poly1 = CurEdge->GetPoly1();

					static TArray<FNavMeshEdgeBase*> EdgesInGroup;
					EdgesInGroup.Reset();

					CurEdge->GetAllStaticEdgesInGroup(Poly0, EdgesInGroup);
					
					// find the longest edge in the group
					FLOAT BestLength = 0.f;
					INT BestIdx = 0;
					for(INT EdgeGroupIdx=0;EdgeGroupIdx<EdgesInGroup.Num();++EdgeGroupIdx)
					{	
						FNavMeshEdgeBase* CurInnerEdge = EdgesInGroup(EdgeGroupIdx);
												
						if( CurInnerEdge != NULL )
						{
							FLOAT CurLen = CurInnerEdge->GetEdgeLength();

							if (CurLen > BestLength)
							{
								BestIdx = EdgeGroupIdx;
								BestLength = CurLen;
							}
						}
					}

					FNavMeshEdgeBase* LongestEdgeInGroup = EdgesInGroup(BestIdx);

					if (CurEdge != LongestEdgeInGroup)
					{
						continue;
					}

					if ( Poly0 != NULL && Poly1 != NULL )
					{
						EdgesToConsider.AddItem(FEdgeEdgeIDPair(CurEdgeID,CurEdge));
					}
				}

				// make sure that we have at least one edge to work on
				// (RecastSnap step may add a new vertex and divide edge, but both parts will still be on a line)
				if (EdgesToConsider.Num() == 0)
				{
					for (INT EdgePolyIdx=0;EdgePolyIdx<CurPoly.PolyEdges.Num();++EdgePolyIdx)
					{
						WORD CurEdgeID = CurPoly.PolyEdges(EdgePolyIdx);
						FNavMeshEdgeBase* CurEdge = MeshThatOwnsTheEdges->GetEdgeAtIdx(CurEdgeID);
						FNavMeshPolyBase* Poly0 = CurEdge->GetPoly0();
						FNavMeshPolyBase* Poly1 = CurEdge->GetPoly1();

						if ( Poly0 != NULL && Poly1 != NULL )
						{
							EdgesToConsider.AddItem(FEdgeEdgeIDPair(CurEdgeID,CurEdge));
							break;
						}
					}
				}

				SortedEdgeList SegmentEdges;

				FLOAT Height = 0.f;
				FVector HeightOffset(0.f);

				// now that we have a list of relevant edges, add obstacle poly geo for each edge
				for(INT EdgeToConsiderIdx=0;EdgeToConsiderIdx<EdgesToConsider.Num();++EdgeToConsiderIdx)
				{
					FNavMeshCrossPylonEdge* CurEdgeToAddFor = static_cast<FNavMeshCrossPylonEdge*>(EdgesToConsider(EdgeToConsiderIdx).Edge);
					FVector EdgeVert0 = CurEdgeToAddFor->GetVertLocation(0,LOCAL_SPACE);
					FVector EdgeVert1 = CurEdgeToAddFor->GetVertLocation(1,LOCAL_SPACE);

					// need to make sure winding is correct, so flip the vert order if they're in the wrong direction
					if( ((EdgeVert1-EdgeVert0) | (PolySegmentPt1 - PolySegmentPt0)) < 0.f )
					{
						FVector Tmp = EdgeVert0;
						EdgeVert0 = EdgeVert1;
						EdgeVert1 = Tmp;
					}

					// expand verts out by supported edge width so the obstacle mesh represents the walkable space (edges that support size n can supports a guy of size n moving through them along any point of the edge)
					if( CurEdgeToAddFor->GetEdgeType() != NAVEDGE_DropDown ) 
					{
						const FVector EdgeDir = (EdgeVert0-EdgeVert1).SafeNormal();
						EdgeVert0 += EdgeDir * Min<FLOAT>(CurEdgeToAddFor->EffectiveEdgeLength,(EdgeVert0-PolySegmentPt0).Size());
						EdgeVert1 -= EdgeDir * Min<FLOAT>(CurEdgeToAddFor->EffectiveEdgeLength,(EdgeVert1-PolySegmentPt1).Size());

					}

					// find a good height for new poly
					Height = Max<FLOAT>(CurEdgeToAddFor->GetPoly0()->GetPolyHeight(),CurEdgeToAddFor->GetPoly1()->GetPolyHeight());

					// construct new poly and add it to the mesh
					HeightOffset = FVector(0.f,0.f,Height);
					static TArray<FVector> NewPolyPts;
					NewPolyPts.Reset();

					NewPolyPts.AddItem(EdgeVert1+HeightOffset);
					NewPolyPts.AddItem(EdgeVert1);
					NewPolyPts.AddItem(EdgeVert0);
					NewPolyPts.AddItem(EdgeVert0+HeightOffset);					

					FNavMeshPolyBase* NewPoly = AddPoly(NewPolyPts,200.f,LOCAL_SPACE);
					if (NewPoly)
					{
						// link new poly to this edge
						NewPoly->PolyEdges.AddItem(EdgesToConsider(EdgeToConsiderIdx).EdgeID);
						CurEdgeToAddFor->ObstaclePolyID = NewPoly->Item;
					}

					// save off this edge in the sorted edge list so we can add blank polys for the gaps
					InsertSortedEdgeForSeg(EdgeVert0,EdgeVert1,PolySegmentPt0,SegmentEdges,MeshThatOwnsTheEdges);
				}			

				// now add geo for the gaps
				// MT-> Pass NULL here so we don't add redundant (and wrong) data to borderedgelist, stuff added to borderedge list on first pass of obstacle mesh will suffice
				// Segment points in LOCAL_SPACE
				AddObstacleGeoForPolySegment(MeshThatOwnsTheEdges,this,PolySegmentPt0,PolySegmentPt1,SegmentPt0, SegmentPt1, SegmentEdges,HeightOffset,NULL, NULL, LOCAL_SPACE);
			}
		}
	}

	// now that we have a new structure in the build arrays, clear out old poly list and rebuild it
	Polys.Empty();

	// finalize
	FixupForSaving();
	KDOPInitialized=FALSE;
	BuildKDOP();
}

/**
 * will add appropriate obstacle mesh geometry for path obstacles who split the mesh
 * called after edges are created because we don't want to pollute 
 * @param Mesh - the (walkable) navmesh we are adding geo for
 */
void UNavigationMeshBase::BuildObstacleGeoForPathobjects(UNavigationMeshBase* Mesh)
{
	if( Polys.Num() > 0 )
	{
		BuildPolys.Clear();
		for( INT VertIdx = 0; VertIdx < Verts.Num(); ++VertIdx )
		{
			Verts(VertIdx).ContainingPolys.Empty();
		}

		// for each existing poly either copy it directly or add several new polys for it
		for( INT PolyIdx = 0; PolyIdx < Polys.Num(); ++PolyIdx )
		{			
			FNavMeshPolyBase& CurPoly = Polys(PolyIdx);
			if( Polys(PolyIdx).OctreeId.IsValidId() )
			{
				RemovePolyFromOctree(&Polys(PolyIdx));
			}

			AddPolyFromVertIndices(Polys(PolyIdx).PolyVerts,Polys(PolyIdx).PolyHeight);
		}
	}

	// give pathobjects a chance to add obstacle geo
	TArray< FNavMeshPolyBase* > AddedPolys;
	for(INT PathObjectIdx=0;PathObjectIdx<PathObjects.Num();++PathObjectIdx)
	{
		IInterface_NavMeshPathObject* PO = PathObjects(PathObjectIdx);
		static TArray<FNavMeshPolyBase*> Polys;
		Polys.Reset();

		static TArray<FVector> PolyShape;
		PolyShape.Reset();

		FLOAT height(0.f);
		if( !PO->GetMeshSplittingPoly(PolyShape,height) )
		{
			continue;
		}


		// if htis PO intersects a poly in the walkable mesh, add its geo to the obstacle mesh
		FNavMeshPolyBase* Poly=NULL;
		if( Mesh->IntersectsPoly(PolyShape,Poly,NULL,WORLD_SPACE) ) 
		{
			// then add this shape's segments to the obstacle mesh
			for(INT ShapeVertIdx=0;ShapeVertIdx<PolyShape.Num();++ShapeVertIdx)
			{
				// outward facing polys
				FVector CurVert = PolyShape(ShapeVertIdx);
				FVector NextVert = PolyShape((ShapeVertIdx+1)%PolyShape.Num() );

				FVector Ctr = (CurVert+NextVert)*0.5f;
				FVector Offset = ((NextVert-CurVert).SafeNormal() ^ FVector(0.f,0.f,1.f)).SafeNormal() * NAVMESHGEN_STEP_SIZE;
				FNavMeshPolyBase* PolyForEdge = Mesh->GetPolyFromPoint(Ctr+Offset,NAVMESHGEN_MIN_WALKABLE_Z,WORLD_SPACE);

				if (PolyForEdge != NULL )
				{
					FVector CurVert_AtGround = CurVert;
					PolyForEdge->AdjustPositionToDesiredHeightAbovePoly(CurVert_AtGround,0.f);
					FVector NextVert_AtGround = NextVert;
					PolyForEdge->AdjustPositionToDesiredHeightAbovePoly(NextVert_AtGround,0.f);
					if ( PolyForEdge != NULL )
					{
						Offset = FVector(0.f,0.f,PolyForEdge->PolyHeight);
						TArray<FVector> NewObstaclePoly;

						NewObstaclePoly.AddItem(CurVert_AtGround);
						NewObstaclePoly.AddItem(NextVert_AtGround);
						NewObstaclePoly.AddItem(NextVert_AtGround+Offset);
						NewObstaclePoly.AddItem(CurVert_AtGround+Offset);

						FNavMeshPolyBase* NewPoly = AddPoly(NewObstaclePoly,200.f,WORLD_SPACE);
						if( NewPoly != NULL )
						{
							AddedPolys.AddItem(NewPoly);
						}
					}
				}
			}
		}
	}

	// now that we have a new structure in the build arrays, clear out old poly list and rebuild it
	Polys.Empty();

	// finalize
	FixupForSaving();
	KDOPInitialized=FALSE;
	BuildKDOP();
}


void TestCollision(UNavigationMeshBase* BaseMesh, UNavigationMeshBase* ObstacleMesh)
{
	// for each poly in base mesh
	for(INT BaseIdx=0;BaseIdx<BaseMesh->Polys.Num();BaseIdx++)
	{
		FNavMeshPolyBase* CurBasePoly = &BaseMesh->Polys(BaseIdx);

		for(INT InnerBaseIdx=0;InnerBaseIdx<BaseMesh->Polys.Num();InnerBaseIdx++)
		{
			FNavMeshPolyBase* CurInnerBasePoly = &BaseMesh->Polys(InnerBaseIdx);
			if(CurInnerBasePoly == CurBasePoly || appFrand() > 0.1f)
			{
				continue;
			}

			FCheckResult Hit(1.f);
			FVector Start = CurInnerBasePoly->CalcCenter(WORLD_SPACE)+FVector(0.f,0.f,NAVMESHGEN_ENTITY_HALF_HEIGHT);
			FVector End = CurBasePoly->CalcCenter(WORLD_SPACE)+FVector(0.f,0.f,NAVMESHGEN_ENTITY_HALF_HEIGHT);

			if(!ObstacleMesh->LineCheck(BaseMesh, Hit,End,Start,FVector(NAVMESHGEN_STEP_SIZE,NAVMESHGEN_STEP_SIZE,NAVMESHGEN_ENTITY_HALF_HEIGHT),0))
			{
				GWorld->GetWorldInfo()->DrawDebugLine(Start,End,255,0,0,TRUE);
				GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(Hit.Location,FRotator(0,0,0),50.f,TRUE);
			}
			else
			{
				GWorld->GetWorldInfo()->DrawDebugLine(Start,End,0,255,0,TRUE);

			}
		}
	}
}

// overridden to clear navmesh data, called from DefinePaths
void APylon::ClearPaths()
{
	Super::ClearPaths();
	NextPylon=NULL;
}

void APylon::ClearNavMeshPathData()
{
	if(!bImportedMesh && bBuildThisPylon)
	{
		// loop through adjacent pylons which link to this one and remove crosspylon edges that touch me so that if only this 
		// pylon is being built there won't be dangling references
		for(INT ReachSpecIdx=0;ReachSpecIdx<PathList.Num();++ReachSpecIdx)
		{
			UReachSpec* Spec = PathList(ReachSpecIdx);
			APylon* Pylon = Cast<APylon>(~(Spec->End));
			if( Pylon != NULL && Pylon->NavMeshPtr != NULL)
			{
				for(INT CPIdx=Pylon->NavMeshPtr->CrossPylonEdges.Num()-1;CPIdx>=0;--CPIdx)
				{
					FNavMeshCrossPylonEdge* CPEdge = Pylon->NavMeshPtr->CrossPylonEdges(CPIdx);
					if( ~CPEdge->Poly0Ref.OwningPylon == this ) 
					{
						CPEdge->Poly0Ref.OwningPylon = NULL;
					}

					if( ~CPEdge->Poly1Ref.OwningPylon == this )
					{
						CPEdge->Poly1Ref.OwningPylon = NULL;
					}
				}
			}
		}
		
		NavMeshPtr=NULL;
		ObstacleMesh=NULL;
		NextPassSeedList.Empty();
	}
}

/**
*	Do anything needed to clear out cross level references; Called from ULevel::PreSave
*/

void APylon::ClearCrossLevelReferences()
{
	Super::ClearCrossLevelReferences();

	if(NavMeshPtr != NULL)
	{
		for(INT Idx=0;Idx<NavMeshPtr->GetNumEdges();++Idx)
		{
			FNavMeshEdgeBase* Edge = NavMeshPtr->GetEdgeAtIdx(Idx);
			if( Edge != NULL && Edge->ClearCrossLevelReferences() )
			{
				bHasCrossLevelPaths = TRUE;
			}			
		}

		for( INT PolyIdx = 0; PolyIdx < NavMeshPtr->Polys.Num(); PolyIdx++ )
		{
			FNavMeshPolyBase& Poly = NavMeshPtr->Polys(PolyIdx);
			for( INT CovIdx = 0; CovIdx < Poly.PolyCover.Num(); CovIdx++ )
			{
				FCoverReference& CovRef = Poly.PolyCover(CovIdx);
				ACoverLink* Link = Cast<ACoverLink>(*CovRef);
				if( Link != NULL && Link->GetOutermost() != GetOutermost())
				{
					bHasCrossLevelPaths=TRUE;
					Link->SetOwner( NULL );
				}

				if( *CovRef == NULL && !CovRef.Guid.IsValid() )
				{
					Poly.RemoveCoverReference( CovIdx-- );
					continue;
				}

				if( *CovRef != NULL && GetOutermost() != CovRef->GetOutermost() )
				{
					bHasCrossLevelPaths=TRUE;
					CovRef.Guid = *CovRef->GetGuid();
				}
			}
		}
	}
}

static FLOAT FindEndPylon( ANavigationPoint* CurrentNode, APawn* seeker, FLOAT bestWeight )
{
	if ( CurrentNode->bEndPoint )
	{
		return 1.f;
	}
	else
		return 0.f;
}

UBOOL APylon::CostFor(const FNavMeshPathParams& PathParams,
					  const FVector& PreviousPoint,
					  FVector& out_PathEdgePoint,
 					  struct FNavMeshEdgeBase* Edge,
					  struct FNavMeshPolyBase* SourcePoly,
					  INT& out_Cost)
{
  return FALSE;
}

UBOOL APylon::CanReachPylon( APylon* DestPylon, AController* C )
{
	if( DestPylon == NULL )
		return FALSE;
	if( DestPylon == this )
		return TRUE;	

	UBOOL bResult = FALSE;
	if( C != NULL && C->Pawn != NULL )
	{
		C->Pawn->InitForPathfinding( DestPylon, DestPylon );
		for (ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint)
		{
			Nav->ClearForPathFinding();
		}
		visitedWeight = 0;
		DestPylon->bEndPoint = TRUE;

		FLOAT BestWeight = 0.f;
		ANavigationPoint* BestDest = C->Pawn->BestPathTo( &FindEndPylon, this, &BestWeight, FALSE, UCONST_BLOCKEDPATHCOST, 0 );
		bResult = (BestDest != NULL);
	}

	return bResult;
}

void FNavMeshCrossPylonEdge::GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel, UBOOL bIsDynamic)
{
	// if the edge is dynamic and a level is being removed don't emit refs becuase we don't want refs cleared prematurely.  Dynamic edges will take care of their own ref clearing
	if(!(bIsDynamic && bIsRemovingLevel))
	{
		if( (bIsRemovingLevel && Poly0Ref.OwningPylon.Actor != NULL) || 
			(!bIsRemovingLevel && Poly0Ref.OwningPylon.Actor == NULL))
		{
			ActorRefs.AddItem(&Poly0Ref.OwningPylon);
		}


		if( (bIsRemovingLevel && Poly1Ref.OwningPylon.Actor != NULL) || 
			(!bIsRemovingLevel && Poly1Ref.OwningPylon.Actor == NULL))
		{
			ActorRefs.AddItem(&Poly1Ref.OwningPylon);
		}

		if( bIsRemovingLevel )
		{
			Poly0Ref.ClearCachedPoly();
			Poly1Ref.ClearCachedPoly();
		}
	}
}

/**
 * called when levels are being saved and cross level refs need to be cleared
 */
UBOOL FNavMeshCrossPylonEdge::ClearCrossLevelReferences()
{
	if(Poly0Ref.OwningPylon.Actor != NULL && Poly1Ref.OwningPylon.Actor != NULL)
	{
		if(Poly0Ref.OwningPylon.Actor->GetOutermost() != Poly1Ref.OwningPylon.Actor->GetOutermost())
		{
			Poly0Ref.OwningPylon.Guid = *Poly0Ref.OwningPylon.Actor->GetGuid();
			Poly0Ref.OwningPylon.Actor = NULL;
			Poly1Ref.OwningPylon.Guid = *Poly1Ref.OwningPylon.Actor->GetGuid();
			Poly1Ref.OwningPylon.Actor = NULL;
		}
		else
		{
			return FALSE;
		}
	}

	return TRUE;
}

void FNavMeshSpecialMoveEdge::GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel, UBOOL bIsDynamic)
{
	FNavMeshCrossPylonEdge::GetActorReferences(ActorRefs,bIsRemovingLevel,bIsDynamic);

	if( (bIsRemovingLevel && RelActor.Actor != NULL) || 
		(!bIsRemovingLevel && RelActor.Actor == NULL))
	{
		ActorRefs.AddItem(&RelActor);
	}
}

/**
 * called when levels are being saved and cross level refs need to be cleared
 */
UBOOL FNavMeshSpecialMoveEdge::ClearCrossLevelReferences()
{
	UBOOL bSuperRetVal = FNavMeshCrossPylonEdge::ClearCrossLevelReferences();

	APylon* MyPylon = NavMesh->GetPylon();
	if( MyPylon == NULL )
	{
		return bSuperRetVal;
	}

	if( RelActor.Actor != NULL )
	{
		if(RelActor->GetOutermost() != MyPylon->GetOutermost())
		{
			RelActor.Guid = *RelActor.Actor->GetGuid();
			RelActor.Actor = NULL;			
			bSuperRetVal = TRUE;
		}
	}

	if( MoveDest.Base != NULL )
	{
		if( MoveDest.Base->GetOutermost() != MyPylon->GetOutermost() )
		{
			debugf(TEXT("WARNING! SpecialMoveEdge.MoveDest is based on an actor (%s) not in the same level as my pylon! (%s).. Base is being set to NULL"),*MoveDest.Base->GetName(), *MyPylon->GetName());
			FVector World_Loc = *MoveDest;
			MoveDest.Set(NULL,World_Loc);
		}
	}

	return bSuperRetVal;
}

/**
* Called when a level is loaded/unloaded, to get a list of all the crosslevel
* paths that need to be fixed up.
*/
void APylon::GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel)
{
	Super::GetActorReferences(ActorRefs,bIsRemovingLevel);

	if(NavMeshPtr != NULL)
	{
		
		for(INT EdgeIdx=0;EdgeIdx<NavMeshPtr->GetNumEdges();EdgeIdx++)
		{
			FNavMeshEdgeBase* CurEdge = NavMeshPtr->GetEdgeAtIdx(EdgeIdx);
			CurEdge->GetActorReferences(ActorRefs,bIsRemovingLevel,FALSE);
		}

		for(DynamicEdgeList::TIterator Itt(NavMeshPtr->DynamicEdges);Itt;++Itt)
		{
			FNavMeshEdgeBase* CurEdge = Itt.Value();
			CurEdge->GetActorReferences(ActorRefs,bIsRemovingLevel,TRUE);
		}

		for( INT PolyIdx = 0; PolyIdx < NavMeshPtr->Polys.Num(); PolyIdx++ )
		{
			FNavMeshPolyBase& Poly = NavMeshPtr->Polys(PolyIdx);
			Poly.GetActorReferences(ActorRefs,bIsRemovingLevel);
		}
	}
}

/**
 * called by APylon::GetActorReferences, allows each poly to supply its own refs
 */
void FNavMeshPolyBase::GetActorReferences(TArray<FActorReference*>& ActorRefs, UBOOL bIsRemovingLevel)
{
	for( INT CovIdx = 0; CovIdx < PolyCover.Num(); CovIdx++ )
	{
		FCoverReference& CovRef = PolyCover(CovIdx);
		if( (bIsRemovingLevel && CovRef.Actor != NULL) ||
			(!bIsRemovingLevel && CovRef.Actor == NULL) )
		{
			ActorRefs.AddItem( &CovRef );
		}
	}

	// if we have a submesh, call this on all its polys
	UNavigationMeshBase* SubMesh = GetSubMesh();
	
	if (SubMesh != NULL )
	{
		for( INT PolyIdx = 0; PolyIdx < SubMesh->Polys.Num(); ++PolyIdx )
		{
			FNavMeshPolyBase& Poly = SubMesh->Polys(PolyIdx);
			Poly.GetActorReferences(ActorRefs,bIsRemovingLevel);
		}
	}
}

FPolyReference::FPolyReference(FNavMeshPolyBase* InPoly)
{
	UNavigationMeshBase* Mesh = InPoly->NavMesh;
	APylon* Pylon = Mesh->GetPylon();

	WORD SubPolyId = MAXWORD;
	WORD TopLevelPolyId = MAXWORD;
	// if poly is in a submesh then offset poly ID 
	if(Mesh->IsSubMesh())
	{
		SubPolyId = InPoly->Item;

		// determine parent poly ID	
		UNavigationMeshBase* ParentMesh = Mesh->GetTopLevelMesh();
		WORD* ParentID = ParentMesh->SubMeshToParentPolyMap.Find(Mesh);
		checkSlowish(ParentID != NULL);
		TopLevelPolyId = *ParentID;

	}
	else
	{
		TopLevelPolyId = InPoly->Item;
	}

	SetPolyId(TopLevelPolyId,SubPolyId);
	OwningPylon = FActorReference(Pylon,*Pylon->GetGuid());
}

FNavMeshPolyBase* FPolyReference::operator*()
{
	return GetPoly();
}

/**
 * this will dereference the poly and return a pointer to it
 * @param bEvenIfPylonDisabled - pass TRUE to this if you want the poly even if its pylon is bDisabled
 * @return the poly assoicated with this poly ref
 */
FNavMeshPolyBase* FPolyReference::GetPoly(UBOOL bEvenIfPylonDisabled/*=FALSE*/)
{
	APylon* RESTRICT Pylon = (APylon*)(*OwningPylon);
	
	if( Pylon == NULL || (!bEvenIfPylonDisabled && Pylon->bDisabled) )
	{
		CachedPoly = NULL;
		return NULL;
	}

	// if we don't have a cace value yet, look it up
	if( CachedPoly != NULL )
	{
		return CachedPoly;
	}
	
	FNavMeshPolyBase* ReturnPoly = NULL;
	if(Pylon != NULL && Pylon->NavMeshPtr != NULL && (bEvenIfPylonDisabled || !Pylon->bDisabled))
	{
		WORD SubPolyId = GetSubPolyId();
		WORD TLPolyId = GetTopLevelPolyId();
		FNavMeshPolyBase* TLPoly = Pylon->NavMeshPtr->GetPolyFromId(TLPolyId);
		
		if( TLPoly != NULL )
		{
			// if this mesh has subpoly and we have a valid subpolyid
			if(TLPoly->NumObstaclesAffectingThisPoly > 0 && SubPolyId != MAXWORD)
			{
				UNavigationMeshBase* SubMesh = TLPoly->GetSubMesh();
				if(SubMesh != NULL)
				{
					ReturnPoly = SubMesh->GetPolyFromId(SubPolyId);
				}
			}
			else if(SubPolyId==MAXWORD)
			{
				ReturnPoly = TLPoly;
			}
		}
	}

	CachedPoly = ReturnPoly;
	return CachedPoly;
}

UBOOL FPolyReference::operator==(FNavMeshPolyBase* Poly) const
{
	if( OwningPylon.Actor == Poly->NavMesh->GetPylon() )
	{
		WORD SubPolyID = GetSubPolyId();
		WORD TLPolyID = GetTopLevelPolyId();
		if( SubPolyID != MAXWORD )
		{
			// if the sub poly ID matches out poly's ID, double check our Poly's obstacleinfo to ensure this is a match (do this last 
			if( Poly->Item == SubPolyID )
			{
				FPolyObstacleInfo* ObstInfo = Poly->GetObstacleInfo();

				if( ObstInfo != NULL && ObstInfo->Poly != NULL && ObstInfo->Poly->Item == TLPolyID )
				{
					return TRUE;
				}
			}
		}
		else if ( TLPolyID == Poly->Item )
		{
			return TRUE;
		}
	}
	return FALSE;
}

FPolyReference::operator UBOOL()
{
	return (*(*this) != NULL);
}
UBOOL FPolyReference::operator!()
{
	return (*(*this) == NULL);
}


APylon* FPolyReference::Pylon()
{
	return Cast<APylon>(*OwningPylon);
}

FPolyReference* FPolyReference::operator=(FNavMeshPolyBase* Poly)
{
	*this = FPolyReference(Poly);
	return this;
}

UBOOL APylon::DoesCoverSlotAffectMesh( const FCoverInfo& Slot )
{
	return Slot.Link->Slots(Slot.SlotIdx).bCanMantle || Slot.Link->Slots(Slot.SlotIdx).bCanClimbUp;
}

void APylon::GatherCoverReferences( AScout* Scout, TArray<FCoverInfo>& out_MeshAffectors )
{
	UNavigationMeshBase* NavMesh = GetNavMesh();
	if( NavMesh == NULL )
		return;

	for( INT PolyIdx = 0; PolyIdx < NavMesh->Polys.Num(); PolyIdx++ )
	{
		FNavMeshPolyBase& Poly = NavMesh->Polys(PolyIdx);
		Poly.PolyCover.Empty();
	}

	for( ACoverLink *Link = GWorld->GetWorldInfo()->CoverList; Link != NULL; Link = Link->NextCoverLink )
	{

		for( INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++ )
		{
			
			if( !Link->LinkCoverSlotToNavigationMesh(SlotIdx,NavMesh))
			{
				continue;
			}

			// if this slot affects the mesh put it in the list to be asked for mesh seeds later
			FCoverInfo Info(Link,SlotIdx);
			if(DoesCoverSlotAffectMesh(Info))
			{
				out_MeshAffectors.AddItem(Info);
			}

			// Make the owner of the Link the pylon for reference later
			// this is here so that when further exploration is done (e.g. from a mantleup) we know
			// which pylon to add expansion points to
			Link->SetOwner( this );
		}
	}
}

/**
 * given the passed point, will adjust the point to the passed height obove this poly (in a cardinal direction)
 * @param out_Pt - point to adjust
 * @param DesiredHeight - desired offset from poly
 * @param bWorldSpace - whether passed in out_Pt is in worldspace, else local (default == TRUE)
 */
void FNavMeshPolyBase::AdjustPositionToDesiredHeightAbovePoly(FVector& out_Pt, FLOAT DesiredHeight, UBOOL bWorldSpace)
{
	// Find distance along chosen axis to plane, then move back along chosen axis half poly height, minus distance to plane
	const FVector PolyNorm = GetPolyNormal();

	FLOAT Best=-10.f;
	INT BestIdx=-1;

	// find direction most closely aligned with normal
	FLOAT Current = 0.f;
	for(INT Idx=0;Idx<3;++Idx)
	{
		Current = Abs<FLOAT>(PolyNorm[Idx]);
		if( Current > Best )
		{
			BestIdx = Idx;
			Best = Current;
		}
	}
	FVector Dir(0.f);
	if( PolyNorm[BestIdx] > 0.f )
	{
		Dir[BestIdx] = 1.f;
	}
	else if( PolyNorm[BestIdx] < 0.f )
	{
		Dir[BestIdx] = -1.f;
	}
	// planedot for signed dist to plane
	// divide plandotres by dir|planenorm
	// add dir*result of divide to spot
	const FPlane PolyPlane = GetPolyPlane( bWorldSpace );

	const FLOAT DirPlaneDot = Dir | PolyPlane;

	const FLOAT Dist = PolyPlane.PlaneDot(out_Pt)/DirPlaneDot;

	out_Pt = out_Pt - (Dir * (Dist-DesiredHeight));
}

/** 
* Adds a cover reference to this polygon
* @param Ref - the cover reference to add
*/
void FNavMeshPolyBase::AddCoverReference(const FCoverReference& Ref)
{
	PolyCover.AddItem(Ref);

	// if this cover refernece is in another level ensure we're in the cross level actor list
	APylon* MyPylon = NavMesh->GetPylon();
	if( MyPylon != NULL &&
		!MyPylon->bHasCrossLevelPaths &&
		(Ref.Actor == NULL || Ref.Actor->GetOutermost() != MyPylon->GetOutermost()) )
	{
		// ensure we are in the cross level actor list
		ULevel* const Level = MyPylon->GetLevel();
		Level->CrossLevelActors.AddItem( MyPylon );
		MyPylon->bHasCrossLevelPaths = TRUE;
	}

}

/**
* looks for the passed cover reference in this poly's poly cover array
* @param Ref - the cover reference to remove
* @return - whether the passed reference was found, and removed
*/
UBOOL FNavMeshPolyBase::RemoveCoverReference(const FCoverReference& Ref)
{
	for(INT Idx=0;Idx<PolyCover.Num(); ++Idx)
	{
		if(PolyCover(Idx) == Ref)
		{
			RemoveCoverReference(Idx);
			return TRUE;
		}
	}

	return FALSE;
}

void FNavMeshPolyBase::RemoveAllCoverReferencesToCoverLink(ACoverLink* Link)
{
	for(INT Idx=PolyCover.Num()-1;Idx>=0; --Idx)
	{
		if(PolyCover(Idx).Actor == Link)
		{
			RemoveCoverReference(Idx);
		}
	}
}

/**
 *	RemoveVertexAtLocalIdx
 *  -removes a vertex from this polygon (based on the local index of the vert)
 * @param LocalIdx - local index of the vert to remove
 * @param bDontRemoveFromList - when TRUE the vert will not be removed from the poly list, but this poly will be removed from its containingpoly list if needed
 */
void FNavMeshPolyBase::RemoveVertexAtLocalIdx(INT LocalIdx, UBOOL bDontRemoveFromList)
{
	checkSlowish(LocalIdx > -1 && LocalIdx < PolyVerts.Num());
	// count how many times the vert at this index is in the poly.. if it's 1, remove this poly from that vert's containing poly array
	INT Count = 0;
	for(INT Idx=0;Idx<PolyVerts.Num();++Idx)
	{
		if(PolyVerts(Idx) == PolyVerts(LocalIdx))
		{
			if(++Count > 1)
			{
				break;
			}
		}
	}
	if( Count <= 1)
	{
		NavMesh->Verts(PolyVerts(LocalIdx)).ContainingPolys.RemoveItem(this);
	}

	if( !bDontRemoveFromList )
	{
		PolyVerts.Remove(LocalIdx);
	}
}

/**
 *	RemoveVertex
 *  - will remove ALL instances of the passed VERTID from this polygon
 *  @param VertexID - VERTID of the vertex we should remove from this poly
 */
void FNavMeshPolyBase::RemoveVertex(VERTID VertexID)
{
	NavMesh->Verts(VertexID).ContainingPolys.RemoveItem(this);
	PolyVerts.RemoveItem(VertexID);
}


/**
* removes the cover reference at the passed index
* @param Idx - the index of the cover reference to remove
*/
void FNavMeshPolyBase::RemoveCoverReference(INT Idx)
{
	// if we have a submesh, we need to remove refs from all its polys to this cover as well
	UNavigationMeshBase* SubMesh = GetSubMesh();
	if( SubMesh != NULL )
	{
		FCoverReference& CovRef = PolyCover(Idx);

		for(INT PolyIdx=0;PolyIdx<SubMesh->Polys.Num();++PolyIdx)
		{
			FNavMeshPolyBase* CurrentPoly = &SubMesh->Polys(PolyIdx);

			for(INT PolyCoverIdx=CurrentPoly->PolyCover.Num()-1;PolyCoverIdx>=0;--PolyCoverIdx)
			{
				FCoverReference& SubMeshCovRef = CurrentPoly->PolyCover(PolyCoverIdx);
				if ( CovRef.Guid == SubMeshCovRef.Guid && CovRef.SlotIdx == SubMeshCovRef.SlotIdx)
				{
					CurrentPoly->RemoveCoverReference(PolyCoverIdx);
				}
			}
		}
	}

	PolyCover.RemoveSwap(Idx);

}

void ClearCoverReferences()
{
	for( ANavigationPoint* N = GWorld->GetFirstNavigationPoint(); N != NULL; N = N->nextNavigationPoint )
	{
		ACoverLink* Link = Cast<ACoverLink>(N);
		if( Link == NULL )
		{
			continue;
		}

		N->SetOwner(NULL);
	}
}

void APylon::CreateExtraMeshData( AScout* Scout )
{
	TArray<FCoverInfo> Affectors;
	GatherCoverReferences( FPathBuilder::GetScout(), Affectors );

	CreateMantleEdges( Scout );
	CreateCoverSlipEdges( Scout );

	// allow path objects to create edges 
	for(INT PathObjectIdx=0;PathObjectIdx<PathObjects.Num();++PathObjectIdx)
	{
		GInitRunaway(); // path objects may end up calling eventFUNC which will eventually cause runaway loop assert if there are a lot of them
		IInterface_NavMeshPathObject* CurrentPO = PathObjects(PathObjectIdx);
		CurrentPO->CreateEdgesForPathObject( this );
	}
	ForceUpdateComponents();

	ClearCoverReferences();
}

void APylon::CreateMantleEdges( AScout* Scout )
{
	UNavigationMeshBase* NavMesh = GetNavMesh();
	if( NavMesh == NULL )
		return;

	for( INT PolyIdx = 0; PolyIdx < NavMesh->Polys.Num(); PolyIdx++ )
	{
		FNavMeshPolyBase& Poly = NavMesh->Polys(PolyIdx);
		for( INT CoverIdx = 0; CoverIdx < Poly.PolyCover.Num(); CoverIdx++ )
		{
			ACoverLink* Link = Cast<ACoverLink>(Poly.PolyCover(CoverIdx).Actor);
			INT SlotIdx = Poly.PolyCover(CoverIdx).SlotIdx;
			if( Link == NULL || SlotIdx < 0 || SlotIdx > Link->Slots.Num() )
				continue;
			FCoverSlot* Slot = &Link->Slots(SlotIdx);
			if( Slot == NULL || (!Slot->bCanMantle && !Slot->bCanClimbUp) )
				continue;

			INT MantleDir = (Slot->bCanMantle) ? 0 : 1;

			// If more than one slot, try to create a full length edge with the slot to the right
			UBOOL bPointMantleEdge = FALSE;
			if( Link->Slots.Num() > 1 )
			{
				UBOOL bCheckLeftSlot = TRUE;

				INT RightSlotIdx = Link->GetSlotIdxToRight( SlotIdx );
				FCoverSlot* RightSlot = (RightSlotIdx>=0) ? &Link->Slots(RightSlotIdx) : NULL;
				if( RightSlot != NULL )
				{
					INT RightMantleDir = (RightSlot->bCanMantle) ? 0 : 1;
					if( RightMantleDir != MantleDir )
					{
						bCheckLeftSlot = TRUE;
					}
					else
					{
						bCheckLeftSlot = FALSE;
						FVector  LeftSlotLocation	= Link->GetSlotLocation( SlotIdx );
						FVector  RightSlotLocation	= Link->GetSlotLocation( RightSlotIdx );
						FRotator LeftSlotRotation	= Link->GetSlotRotation( SlotIdx );
						FRotator RightSlotRotation	= Link->GetSlotRotation( RightSlotIdx );
						FLOAT	 EdgeLength			= (LeftSlotLocation-RightSlotLocation).Size();

						APylon* RightPylon = NULL;
						FNavMeshPolyBase* RightPoly = NULL;
						UNavigationHandle::GetPylonAndPolyFromPos( RightSlotLocation, NAVMESHGEN_MIN_WALKABLE_Z, RightPylon, RightPoly );

						if( &Poly == RightPoly )
						{
							Scout->CreateMantleEdge( &Poly, LeftSlotLocation, RightSlotLocation, LeftSlotRotation, RightSlotRotation, MantleDir, Link, SlotIdx );
						}
						// Otherwise, need to create two edges one for each src poly
						else
						{
							TArray<FPolySegmentSpan> Spans;
							UNavigationHandle::GetPolySegmentSpanList( LeftSlotLocation, RightSlotLocation, Spans );
							for( INT SpanIdx = 0; SpanIdx < Spans.Num(); SpanIdx++ )
							{
								FPolySegmentSpan& Span = Spans(SpanIdx);
								FLOAT A1 = (Span.P1-LeftSlotLocation).Size() / EdgeLength;
								FLOAT A2 = (Span.P2-LeftSlotLocation).Size() / EdgeLength;
								FRotator R1 = Lerp(LeftSlotRotation,RightSlotRotation,A1);
								FRotator R2 = Lerp(LeftSlotRotation,RightSlotRotation,A2);
								Scout->CreateMantleEdge( Span.Poly, Span.P1, Span.P2, R1, R2, MantleDir, Link, SlotIdx );
							}
						}
					}
				}

				if( bCheckLeftSlot )
				{
					// If can't create an edge to the left
					// Check if the slot to the left will create an edge when it is evaluated
					INT LeftSlotIdx = Link->GetSlotIdxToLeft( SlotIdx );
					if( LeftSlotIdx >= 0 )
					{
						FCoverSlot* LeftSlot = &Link->Slots(LeftSlotIdx);
						INT LeftMantleDir = (LeftSlot->bCanMantle) ? 0 : 1;
						if( LeftSlot == NULL || LeftMantleDir != MantleDir )
						{
							// If it won't, create a single point edge
							bPointMantleEdge = TRUE;
						}
					}
				}
			}
			else
			{
				bPointMantleEdge = TRUE;
			}

			if( bPointMantleEdge )
			{
				FVector  SlotLocation = Link->GetSlotLocation( SlotIdx );
				FRotator SlotRotation = Link->GetSlotRotation( SlotIdx );
				Scout->CreateMantleEdge( &Poly, SlotLocation, SlotLocation, SlotRotation, SlotRotation, MantleDir, Link, SlotIdx );
			}
		}			
	}
}

void APylon::CreateCoverSlipEdges( AScout* Scout )
{
	UNavigationMeshBase* NavMesh = GetNavMesh();
	if( NavMesh == NULL )
	{
		return;
	}

	// loop through all cover slots (V Poly V CoverLink V Slot)
	for( INT PolyIdx = 0; PolyIdx < NavMesh->Polys.Num(); PolyIdx++ )
	{
		FNavMeshPolyBase& Poly = NavMesh->Polys(PolyIdx);
		for( INT CoverIdx = 0; CoverIdx < Poly.PolyCover.Num(); CoverIdx++ )
		{
			ACoverLink* Link = Cast<ACoverLink>(Poly.PolyCover(CoverIdx).Actor);
			INT SlotIdx = Poly.PolyCover(CoverIdx).SlotIdx;
			if( Link == NULL || SlotIdx < 0 || SlotIdx > Link->Slots.Num() )
			{
				continue;
			}

			FCoverSlot* Slot = &Link->Slots(SlotIdx);
			if( Slot == NULL )
			{
				continue;
			}

			FVector SlotLocation = Link->GetSlotLocation(SlotIdx);

			// V cover slips allowed for this slot
			for( INT SlipIdx = 0; SlipIdx < Slot->SlipRefs.Num(); SlipIdx++ )
			{
				FSlotMoveRef& Ref = Slot->SlipRefs(SlipIdx);

				APylon* Pylon = NULL;
				FNavMeshPolyBase* OtherPoly = NULL;

				if(UNavigationHandle::GetPylonAndPolyFromPos(BP2Vect(Ref.Dest),NAVMESHGEN_MIN_WALKABLE_Z,Pylon,OtherPoly))
				{
					UNavigationMeshBase* NavMesh = Pylon->GetNavMesh();
					if( NavMesh == NULL || NavMesh->Verts.Num() >= MAXVERTID )
					{
						continue;
					}
					
					if( OtherPoly == NULL )
					{
						continue;
					}

					if( &Poly == OtherPoly )
					{
						continue;
					}

					// set destination poly reference
					Ref.Poly = FPolyReference(Pylon,Poly.Item);

					// create the edge
					TArray<FNavMeshPolyBase*> ConnectedPolys;
					ConnectedPolys.AddItem( &Poly );
					ConnectedPolys.AddItem( OtherPoly );

					FNavMeshCoverSlipEdge* EdgePtr = NULL;
					Poly.NavMesh->AddOneWayCrossPylonEdgeToMesh<FNavMeshCoverSlipEdge>(SlotLocation,SlotLocation,ConnectedPolys,-1.0f,MAXBYTE,&EdgePtr);
					if( EdgePtr != NULL )
					{
						EdgePtr->MoveDir	= Ref.Direction;
						EdgePtr->MoveDest	= Ref.Dest;
						EdgePtr->RelActor	= Link;
						EdgePtr->RelItem	= SlotIdx;
					}			
				}
			}
		}
	}
}

/**
* Creates a navmesh using the geometry of a given StaticMesh
* 
* @param	StaticMesh		The StaticMesh to be converted into a navmesh
*/
#define SNAP_DIST_SQUARED (VERT_SNAP_DIST * VERT_SNAP_DIST)
void APylon::ConvertStaticMeshToNavMesh( UStaticMesh* StaticMesh, FMatrix& ScaledLocalToWorld )
{
	check(StaticMesh);

	// Get the triangle data from the StaticMesh's LOD 0
	FStaticMeshRenderData& StaticMeshRenderData = StaticMesh->LODModels(0);

	if(NavMeshPtr == NULL)
	{
		NavMeshPtr = Cast<UNavigationMeshBase>(StaticConstructObject(UNavigationMeshBase::StaticClass(),this));
		NavMeshPtr->InitTransform(this);
	}

	if(ObstacleMesh == NULL)
	{
		ObstacleMesh = Cast<UNavigationMeshBase>(StaticConstructObject(UNavigationMeshBase::StaticClass(),this));
		ObstacleMesh->InitTransform(this);
	}

#if !FINAL_RELEASE
	if( StaticMeshRenderData.PositionVertexBuffer.GetNumVertices() > MAXVERTID )
	{
		warnf(NAME_Error,TEXT("%s EXCEEDED MAX NUMBER OF VERTS ALLOWED!\nTry using multiple smaller StaticMeshes"), *StaticMesh->GetFullName());
		return;
	}
#endif

	// copy all vert positions into temp array so we can snap them around
	// MT->Note: this represents positions both for incoming obstacle mesh verts and normal mesh verts, but that's OK! we want
	// them all to be snapped to the existing mesh, and not to existing obstacle mesh
	TArray<FVector> VertexPositions;
	for( UINT Idx=0;Idx<StaticMeshRenderData.PositionVertexBuffer.GetNumVertices();++Idx)
	{
		FVector Pos = ScaledLocalToWorld.TransformFVector(StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Idx));
		
		// snap incoming position to existing points in the mesh
		VERTID ClosestVert=MAXVERTID;
		FLOAT ClosestDistSq = -1.f;
		for( INT VertIdx=0;VertIdx<NavMeshPtr->Verts.Num();++VertIdx)
		{
			FVector WS_VertPos = NavMeshPtr->GetVertLocation(VertIdx,WORLD_SPACE);
			FLOAT DistSq = FDistSquared(WS_VertPos, Pos);
			if(DistSq < SNAP_DIST_SQUARED && (DistSq < ClosestDistSq || ClosestDistSq < 0.f))
			{
				ClosestVert = VertIdx;
				ClosestDistSq = DistSq;
			}
		}

		if(ClosestVert != MAXVERTID)
		{
			Pos = NavMeshPtr->GetVertLocation(ClosestVert,WORLD_SPACE);
		}
		VertexPositions.AddItem(Pos);
	}

	// Add the polys using the IndexBuffer
	for( INT Idx = 0; Idx < StaticMeshRenderData.IndexBuffer.Indices.Num(); Idx += 3 )
	{
		TArray<FVector> Poly;

		INT VertIndex_0 = StaticMeshRenderData.IndexBuffer.Indices(Idx+2);
		INT VertIndex_1 = StaticMeshRenderData.IndexBuffer.Indices(Idx+1);
		INT VertIndex_2 = StaticMeshRenderData.IndexBuffer.Indices(Idx);

		Poly.AddItem(VertexPositions(VertIndex_0));
		Poly.AddItem(VertexPositions(VertIndex_1));
		Poly.AddItem(VertexPositions(VertIndex_2));

		

		UNavigationMeshBase* MeshToAddTo = NavMeshPtr;
		// If all verts are red, this is an obstacle poly and should be added to the ObstacleMesh
		if ( StaticMeshRenderData.ColorVertexBuffer.GetNumVertices() > 0 &&
			 IsObstacleColor(StaticMeshRenderData.ColorVertexBuffer.VertexColor(VertIndex_0)) && 
			 IsObstacleColor(StaticMeshRenderData.ColorVertexBuffer.VertexColor(VertIndex_1)) && 
			 IsObstacleColor(StaticMeshRenderData.ColorVertexBuffer.VertexColor(VertIndex_2)) )
		{
			MeshToAddTo = ObstacleMesh;
		}

		MeshToAddTo->AddPoly(Poly,NAVMESHGEN_MAX_POLY_HEIGHT_PYLON(this),WORLD_SPACE);
	}
}

void APylon::AddStaticMeshesToPylon( TArray<AStaticMeshActor*>& SMActors )
{

	for(INT SMActorIdx=0;SMActorIdx<SMActors.Num();++SMActorIdx)
	{
		AStaticMeshActor* StaticMeshActor = SMActors(SMActorIdx);	

		// All static mesh actors should have one of these!
		check( StaticMeshActor->StaticMeshComponent != NULL );

		// Make sure we have a static mesh
		UStaticMesh* StaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;

		// Get the scaling that was applied to the StaticMeshActor
		FMatrix ScaledLocalToWorld = StaticMeshActor->LocalToWorld();

		if( StaticMesh != NULL )
		{
			ConvertStaticMeshToNavMesh( StaticMesh, ScaledLocalToWorld );
		}
		else
		{
			// No mesh associated with this actor's static mesh component!
			appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "ConvertStaticMeshToNavMesh_NoMeshAssignedToStaticMeshActor" ), *StaticMeshActor->GetName() ) ) );
		}
	}

	NavMeshPtr->MergePolys(FVector(1.f),FALSE);
	
	AddToNavigationOctree();

	// MT->TODO - create obstacle mesh if we have no obstacle mesh
	NavMeshPtr->CreateEdgeConnections(FALSE);
	NavMeshPtr->FixupForSaving();
	ObstacleMesh->BuildKDOP();
	FPathBuilder::DestroyScout();
}

#if WITH_EDITOR
void AScout::AbortNavMeshGeneration( TArray<USeqAct_Interp*>& InterpActs )
{
	PathObjects.Empty();

	// ensure imported meshes don't get blown away (put them back into serializable structures)
	for( APylon* ListPylon = GWorld->GetWorldInfo()->PylonList; ListPylon != NULL; ListPylon = ListPylon->NextPylon )
	{
		// build a list of imported pylons
		if( ListPylon->bImportedMesh )
		{

			// put this pylon back into build structures
			if( ListPylon->NavMeshPtr->BuildPolys.Num() > 0 )
			{
				ListPylon->NavMeshPtr->FixupForSaving();
			}

			if( ListPylon->ObstacleMesh->BuildPolys.Num() > 0 )
			{
				ListPylon->ObstacleMesh->FixupForSaving();
			}
		}
	}

	// Reset the interp actors moved for path building
	RestoreInterpActors( InterpActs );
	UndefinePaths();
	// Restore the collision settings
	SetPathCollision(FALSE);
	GWarn->EndSlowTask();		
}
#endif

/**
 * will build an ordered list of pylons that need to be built in order for the passed pylon to be
 * @param DepList - new ordered list of dependencies for PylonToInsert
 * @param PylonToInsert - first pylon we're considering building 
 */
void BuildDeps(TDoubleLinkedList<APylon*>& DepList, APylon* PylonToInsert)
{
	// find all the other pylons which are inside this one's bounds
	for( APylon* CurPylon = GWorld->GetWorldInfo()->PylonList; CurPylon != NULL; CurPylon = CurPylon->NextPylon )
	{
		if(CurPylon != PylonToInsert &&
			PylonToInsert->IsPtWithinExpansionBounds(CurPylon->Location) &&
			!CurPylon->IsPtWithinExpansionBounds(PylonToInsert->Location)) // make sure they're not inside each other causing badness
		{
			warnf(NAME_Log,TEXT("Pylon %s is within Pylon %s and needs to be built first!"),*CurPylon->GetName(),*PylonToInsert->GetName());
			BuildDeps(DepList,CurPylon);	
		}	
	}

	if(DepList.FindNode(PylonToInsert)==NULL)
	{
		DepList.AddTail(PylonToInsert);
	}
}

/**
 *	Rebuilds nav meshes
 *	@param PassNum			Pass number given.
 *	@param bShowMapCheck	If TRUE, conditionally show the Map Check dialog.
 *  @param bOnlyBuildSelected if TRUE only pylons which are selected will be built
 */
#if WITH_EDITOR

/**
 * called when this pylon is about to be built
 */
void APylon::NotifyPylonBuildStarting()
{
	// ON build disable
	for(INT Idx=0;Idx<OnBuild_DisableCollisionForThese.Num();++Idx)
	{
		AActor* Actor = OnBuild_DisableCollisionForThese(Idx);
		if( Actor != NULL )
		{
			debugf(TEXT("NotifyPylonBuildStopping() Disabling collision for %s"), *Actor->GetName());
			Actor->SetCollision(FALSE,FALSE,TRUE);
		}
	}


	// ON build enable
	for(INT Idx=0;Idx<OnBuild_EnableCollisionForThese.Num();++Idx)
	{
		AActor* Actor = OnBuild_EnableCollisionForThese(Idx);
		if( Actor != NULL )
		{
			debugf(TEXT("NotifyPylonBuildStopping() Enabling collision for %s"), *Actor->GetName());
			Actor->SetCollision(TRUE,TRUE,FALSE);
		}
	}
}

/**
 * called when this pylon is no longer being built 
 * (either becuase it was cancelled or the build is finished)
 */
void APylon::NotifyPylonBuildStopping()
{
	// ON STOP build REVERSE dsiable
	for(INT Idx=0;Idx<OnBuild_DisableCollisionForThese.Num();++Idx)
	{
		AActor* Actor = OnBuild_DisableCollisionForThese(Idx);
		if( Actor != NULL )
		{
			debugf(TEXT("NotifyPylonBuildStopping() Enabling collision for %s"), *Actor->GetName());
			Actor->SetCollision(TRUE,TRUE,FALSE);
		}
	}


	// ON build REVERSE enable
	for(INT Idx=0;Idx<OnBuild_EnableCollisionForThese.Num();++Idx)
	{
		AActor* Actor = OnBuild_EnableCollisionForThese(Idx);
		if( Actor != NULL )
		{
			debugf(TEXT("NotifyPylonBuildStopping() Disabling collision for %s"), *Actor->GetName());
			Actor->SetCollision(FALSE,FALSE,TRUE);
		}
	}
}

UBOOL AScout::GenerateNavMesh( UBOOL bShowMapCheck, UBOOL bOnlyBuildSelected )
{
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("GenNavMesh")), FALSE );

	// reset task time tracking
	ResetTaskStats();

	// Rest map has pathing errors warning.
	GWorld->GetWorldInfo()->bMapHasPathingErrors = FALSE;

	// Build Terrain Collision Data
	for (TObjectIterator<UTerrainComponent> TerrainIt; TerrainIt; ++TerrainIt)
	{
		TerrainIt->BuildCollisionData();
	}


	// remove old paths
	UndefinePaths();

	// Build the navigation lists
	BuildNavLists();

	TArray<APylon*> ImportedPylons;
	DWORD PylonsNeedingBuild = 0;
	INT BeforeTotalEdges=0;
	INT BeforeEdgeSize=0;
	for( APylon* ListPylon = GWorld->GetWorldInfo()->PylonList; ListPylon != NULL; ListPylon = ListPylon->NextPylon )
	{
		ListPylon->PathObjectsThatAffectThisPylon=NULL;
		// build a list of imported pylons
		if( ListPylon->bImportedMesh )
		{
			ImportedPylons.AddItem(ListPylon);
		}
		if( ListPylon->NavMeshPtr != NULL )
		{
			BeforeTotalEdges+=ListPylon->NavMeshPtr->GetNumEdges();
			BeforeEdgeSize+=ListPylon->NavMeshPtr->GetAllocatedEdgeBufferSize();
		}

		// determine if we should be building this pylon right now;
		ListPylon->bBuildThisPylon = ListPylon->ShouldBuildThisPylon(bOnlyBuildSelected);

		if( ListPylon->bBuildThisPylon )
		{
			PylonsNeedingBuild++;

			if( bOnlyBuildSelected )
			{
				FPathBuilder::AddBoxToPathBuildBounds(ListPylon->GetExpansionBounds());
			}

			if( ListPylon->bImportedMesh )
			{
				// put this pylon back into build structures
				ListPylon->NavMeshPtr->CopyDataToBuildStructures();
				ListPylon->ObstacleMesh->CopyDataToBuildStructures();
				ListPylon->NavMeshPtr->MergePolys();
				ListPylon->ObstacleMesh->MergePolys();
			}
		}

		ListPylon->ClearNavMeshPathData();

		TotalNumOfPylons=PylonsNeedingBuild;
	}

	// Position interpolated actors in desired locations for path-building.
	TArray<USeqAct_Interp*> InterpActs;
	UBOOL bProblemsMoving = FALSE;
	UpdateInterpActors(bProblemsMoving,InterpActs);
	UBOOL bBuildCancelled = GEngine->GetMapBuildCancelled() || bProblemsMoving;
	if( TotalNumOfPylons == 0 )
	{
		warnf(NAME_Warning,TEXT("CANCELLING BUILD!! because there are no pylons to build.. nothing to do!!!!!!!"));
		bBuildCancelled = TRUE;
	}
	if( bBuildCancelled )
	{
		appMsgf( AMT_OK, TEXT("Aborting navmesh generation due to previous errors!") );
		AbortNavMeshGeneration( InterpActs );
		return FALSE;
	}

	// Setup all actor collision for path building
	SetPathCollision(TRUE);
	// Disable scout collision for marker addition
	SetCollision( FALSE, FALSE, bIgnoreEncroachers );

	// Add NavigationPoint markers to any actors which want to be marked
	INT ProgressDenominator = FActorIteratorBase::GetProgressDenominator();
	const FString LocalizeBuildPathsDefining( LocalizeUnrealEd(TEXT("GenNavMesh_Markers")) );

	INT UpdateGranularity = ProgressDenominator/5;
	INT LastUpdate = 0;
	for( FActorIterator It; It; ++It )
	{
		if(It.GetProgressNumerator() >= LastUpdate + UpdateGranularity)
		{
			GWarn->StatusUpdatef( It.GetProgressNumerator(), ProgressDenominator, *LocalizeBuildPathsDefining );
			LastUpdate = It.GetProgressNumerator();	
		}

		AActor *Actor = *It;
		Actor->AddMyMarker(this);

		// if this actor implements the path object interface, add it to the list for later processing
		IInterface_NavMeshPathObject* POInt = InterfaceCast<IInterface_NavMeshPathObject>(Actor);
		if(POInt != NULL)
		{
			PathObjects.AddItem(POInt);
		}

		if( GEngine->GetMapBuildCancelled() )
		{
			AbortNavMeshGeneration( InterpActs );
			return FALSE;
		}
	}

	// Setup the scout
	SetCollision(TRUE, TRUE, bIgnoreEncroachers);

	// build/adjust cover
	AScout* Scout = FPathBuilder::GetScout();
	// Adjust cover
	//@note - needs to happen after AddMyMarker in case newly created cover was added
	Scout->Exec( TEXT("ADJUSTCOVER FROMDEFINEPATHS=1") );

	for( ACoverLink *Link = GWorld->GetWorldInfo()->CoverList; Link != NULL; Link = Link->NextCoverLink )
	{
		if( !FPathBuilder::IsBuildingPaths() || FPathBuilder::IsPtWithinPathBuildBounds(Link->Location) )
		{
			for( INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++ )
			{
				Link->BuildSlotInfo( SlotIdx, FALSE, Scout );
			}
		}
	}

	// stat vars
	FLOAT TotalArea = 0.f;
	DWORD   TotalPolys = 0;
	DWORD   TotalPylons = 0;
	DWORD   TotalVerts = 0;
	DWORD   TotalVertSize = 0;
	DWORD   TotalEdges = 0;
	DWORD   TotalEdgeSize = 0;
	DWORD   BuiltPylonCount = 0;
	DWORD   TotalPolySize = 0;

	{// scope for profiling
		SCOPE_QUICK_TIMERX(NavMeshGen,FALSE)

#if WITH_RECAST
		if (GEngine->bUseRecastNavMesh)
		{
			// clean up temporary flags in navpoints
			for (ANavigationPoint* iNav = GWorld->GetFirstNavigationPoint(); iNav != NULL; iNav = iNav->nextNavigationPoint)
			{
				iNav->bAlreadyVisited = FALSE;
			}
		}
#endif

		// quick lookip map of pylons we've built already, so we don't built them again!
		TLookupMap<APylon*> BuiltPylons;

		CurrentPylonIndex = 0;

		// ** For each pylon, go through each build pass 
		// BEGIN main build loop
		for( APylon* ListPylon = GWorld->GetWorldInfo()->PylonList; ListPylon != NULL; ListPylon = ListPylon->NextPylon )
		{
			
			// build a list of pylons that need to be built before this one can be built
			TDoubleLinkedList<APylon*> BuildOrder;
			BuildDeps(BuildOrder,ListPylon);

			// build pylons in build order
			for(TDoubleLinkedList<APylon*>::TIterator Itt(BuildOrder.GetHead());Itt;++Itt)
			{
				APylon* Pylon = *Itt;
				
				// if we've already built this mesh, or the pylon is using imported data and does not need to be built, skip
				if(BuiltPylons.HasKey(Pylon) || Pylon->bImportedMesh || !Pylon->bBuildThisPylon)
				{
					continue; 
				}

				BuiltPylonCount++;
				CurrentPylonIndex++;

				warnf(NAME_Log,TEXT("----------------- Building mesh for %s.%s (%i of %i)-----------------"),*Pylon->GetOutermost()->GetName(),*Pylon->GetName(), BuiltPylonCount, PylonsNeedingBuild);
				
				Pylon->NotifyPylonBuildStarting();

				BuiltPylons.AddItem(Pylon);

				// determine generator for this pylon
				BYTE GeneratorType = NavMeshGenerator_Native;
				if (Pylon->bUseRecast && Pylon->bAllowRecastGenerator)
				{
					GeneratorType = NavMeshGenerator_Recast;
				}

				Pylon->NavMeshGenerator = GeneratorType;

				// build an array of functors to call for pylons that need to be built
				TArray<NavMashPassFunc> PassList;
				GetNavMeshPassList( PassList, Pylon->NavMeshGenerator );

				// go through all build passes
				for( INT PassIdx = 0; PassIdx < PassList.Num(); PassIdx++ )
				{
					bBuildCancelled = bBuildCancelled || GEngine->GetMapBuildCancelled();

					// Abort if the build was cancelled, or if it failed for reasons other than vert overflow
					if( ( !(Pylon->*PassList(PassIdx))() && Pylon->NavMeshPtr->Verts.Num() < MAXVERTID ) || 
						bBuildCancelled )
					{
						Pylon->NotifyPylonBuildStopping();

						// special handling for Recast generated mesh: just skip it, it will be shown in error log
						// aborting entire generation is better solution but will flood logs with every pylon on pending list :/
						if (GeneratorType == NavMeshGenerator_Recast)
							break;

						AbortNavMeshGeneration( InterpActs );
						return FALSE;
					}
					// If the build failed due to vert overflow, move to the next Pylon
					else if( Pylon->NavMeshPtr->Verts.Num() >= MAXVERTID )
					{
						Pylon->NotifyPylonBuildStopping();
						break;
					}
				}

				Pylon->NotifyPylonBuildStopping();

			}
		}
		// END main build loop

#if WITH_RECAST
		if (GEngine->bUseRecastNavMesh)
		{
			// perform postponed steps for every pylon
			// don't bother with notifies since it's purely internal conversion

			TArray<NavMashPassFunc> PostponedPassList;
			PostponedPassList.AddItem(&APylon::NavMeshPass_FixupForSaving);
			PostponedPassList.AddItem(&APylon::NavMeshPass_BuildObstacleMesh);

			for ( APylon* ListPylon = GWorld->GetWorldInfo()->PylonList; ListPylon != NULL; ListPylon = ListPylon->NextPylon )
			{
				if (!ListPylon->bImportedMesh && ListPylon->bBuildThisPylon)
				{
					for (INT PassIdx = 0; PassIdx < PostponedPassList.Num(); PassIdx++)
					{
						(ListPylon->*PostponedPassList(PassIdx))();
					}
				}
			}
		}
#endif

		// now we need to ensure imported meshes are splitting other imported meshes.  Since we skipped them above, go through them all 
		// and split them against each other now
		for( INT ImportedIdx=0;ImportedIdx<ImportedPylons.Num();++ImportedIdx)
		{
			ImportedPylons(ImportedIdx)->NavMeshPtr->SplitMeshForIntersectingImportedMeshes();
		}

		// first call fixup so everyone is happy and has data structures in the right place
		for(INT ImportedPylonIdx=0;ImportedPylonIdx< ImportedPylons.Num();++ImportedPylonIdx)
		{
			ImportedPylons(ImportedPylonIdx)->NavMeshPtr->FixupForSaving();
			ImportedPylons(ImportedPylonIdx)->NavMeshPtr->BuildKDOP();
			ImportedPylons(ImportedPylonIdx)->ObstacleMesh->BuildKDOP();
		}


		for( APylon* ListPylon = GWorld->GetWorldInfo()->PylonList; ListPylon != NULL; ListPylon = ListPylon->NextPylon )
		{
			if(ListPylon->bBuildThisPylon || ListPylon->bImportedMesh)
			{

				// extra data step (after all other steps are complete)
				if( ListPylon != NULL && ListPylon->NavMeshPtr->Verts.Num() < MAXVERTID )
				{
					// build edge connections (needs to be done after obstacle mesh is complete as it does obstacle mesh point checks)
					ListPylon->NavMeshPass_CreateEdgeConnections();

					// reset ref to dropedge mesh if any so it gets GCd
					ListPylon->NavMeshPtr->DropEdgeMesh=NULL;

					ListPylon->CreateExtraMeshData(Scout);
				}

				// set the version that this mesh was generated
				if(ListPylon->NavMeshPtr != NULL)
				{
					ListPylon->NavMeshPtr->VersionAtGenerationTime = VER_LATEST_NAVMESH;
				}
				if(ListPylon->ObstacleMesh != NULL)
				{
					ListPylon->ObstacleMesh->VersionAtGenerationTime = VER_LATEST_NAVMESH;
				}
			}
		}


		// Second rotation thru pylons to create super network, calcuate stats
		{
			SCOPE_QUICK_TIMERX(NavMeshGenStats,FALSE)
			for( APylon* ListPylon = GWorld->GetWorldInfo()->PylonList; ListPylon != NULL; ListPylon = ListPylon->NextPylon )
			{
				APylon* Pylon = ListPylon;
				if( Pylon != NULL && Pylon->NavMeshPtr != NULL && Pylon->NavMeshPtr->Verts.Num() < MAXVERTID )
				{
					if( Pylon->bBuildThisPylon )
					{
						if( !Pylon->NavMeshPass_BuildPylonToPylonReachSpecs() )
						{
							AbortNavMeshGeneration( InterpActs );
							return FALSE;
						}
					}

					TotalPylons++;
					TotalPolys+=Pylon->NavMeshPtr->Polys.Num();
					TotalPolys+=Pylon->ObstacleMesh->Polys.Num();
					TotalEdges+=Pylon->NavMeshPtr->GetNumEdges();
					TotalVerts+=Pylon->NavMeshPtr->Verts.Num();
					TotalVerts+=Pylon->ObstacleMesh->Verts.Num();
					TotalVertSize+=Pylon->NavMeshPtr->Verts.GetAllocatedSize();
					TotalVertSize+=Pylon->ObstacleMesh->Verts.GetAllocatedSize();
					TotalEdgeSize+=Pylon->NavMeshPtr->GetAllocatedEdgeBufferSize();
					TotalPolySize+=Pylon->NavMeshPtr->Polys.GetAllocatedSize();
					TotalPolySize+=Pylon->ObstacleMesh->Polys.GetAllocatedSize();

					// calc area of this pylon
					for(INT PolyIdx=0;PolyIdx<Pylon->NavMeshPtr->Polys.Num();++PolyIdx)
					{
						TotalArea += Pylon->NavMeshPtr->GetPolyFromId(PolyIdx)->CalcArea();
					}
				}

				// Refresh render info
				ListPylon->ForceUpdateComponents(FALSE,FALSE);

			}		
		}
	}// end profiling scope

	GWarn->StatusUpdatef( 10, 10, TEXT("Completed NavMesh") );

	RestoreInterpActors( InterpActs );

	PathObjects.Empty();
		
	// cleanup collision changes
	SetPathCollision(FALSE);

	if(!bBuildCancelled)
	{
		GWorld->GetWorldInfo()->bPathsRebuilt = (!bOnlyBuildSelected);
		INT TotalSize = TotalVertSize+TotalEdgeSize+TotalPolySize;
		debugf(TEXT("---> Navigation mesh built successfully! Here are some stats:"));
		debugf(TEXT("Built mesh for %i out of %i pylons"),BuiltPylonCount,TotalPylons);
		debugf(TEXT("TotalArea: %.2f"),TotalArea);
		debugf(TEXT("TotalPolys: %i Size(bytes): %i %.2f %% of total"),TotalPolys,TotalPolySize,100.0f*(FLOAT)TotalPolySize/(FLOAT)TotalSize);
		debugf(TEXT("TotalEdgeCount: %i Size(bytes): %i %.2f %% of total"),TotalEdges,TotalEdgeSize,100.0f*(FLOAT)TotalEdgeSize/(FLOAT)TotalSize);
		debugf(TEXT("Edge Delta: %i Size(bytes): %i"),TotalEdges-BeforeTotalEdges,TotalEdgeSize-BeforeEdgeSize);
		debugf(TEXT("TotalVertCount: %i Size(bytes): %i %.2f %% of total"),TotalVerts,TotalVertSize,100.0f*(FLOAT)TotalVertSize/(FLOAT)TotalSize);
		debugf(TEXT("Total Mesh Data Size(bytes): %i"),TotalSize);
	}

	// print info about the build we just did
	GWarn->EndSlowTask();		

	// print task tracking stats
	PrintTaskStats();

	CheckForErrors();

	return TRUE;
}
#endif

/**
 * Prepares list of NavMesh generation passes executed for every pylon
 * @param PassList - list of passes
 * @param GeneratorType - NavMesh generator type (check ENavigationMeshGeneratorType for details)
 */
void AScout::GetNavMeshPassList( TArray<NavMashPassFunc>& PassList, BYTE GeneratorType )
{
	if (GeneratorType == NavMeshGenerator_Recast)
	{
		PassList.AddItem( &APylon::NavMeshPass_Recast );
	}
	else
	{
		PassList.AddItem( &APylon::NavMeshPass_InitialExploration );
		PassList.AddItem( &APylon::NavMeshPass_ExpandSeeds );
		PassList.AddItem( &APylon::NavMeshPass_SimplifyMesh );
	}

	PassList.AddItem( &APylon::NavMeshPass_SplitMeshAboutPathObjects );
	PassList.AddItem( &APylon::NavMeshPass_SplitForImportedMeshes );

#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		// postpone building runtime data as post step (before edges), so mesh snapping could operate on build structures
		PassList.AddItem( &APylon::NavMeshPass_RecastSnap );
	}
	else
#endif
	{
		PassList.AddItem( &APylon::NavMeshPass_FixupForSaving );
		PassList.AddItem( &APylon::NavMeshPass_BuildObstacleMesh );
	}
	//MT->NOTE: Edges happen as a post step (after obstacle mesh is built)	
}

//	-- 1st pass nav mesh exploration	
UBOOL APylon::NavMeshPass_InitialExploration()
{
	AScout* Scout = FPathBuilder::GetScout();

	NavMeshPtr = Cast<UNavigationMeshBase>(StaticConstructObject(UNavigationMeshBase::StaticClass(),this));
	NavMeshPtr->InitTransform(this);
	ObstacleMesh = Cast<UNavigationMeshBase>(StaticConstructObject(UNavigationMeshBase::StaticClass(),this));
	ObstacleMesh->InitTransform(this);
	WorkingSetPtr = new WSType();
	PathObjectsThatAffectThisPylon = new TArray<class IInterface_NavMeshPathObject*>();
	PathObjectsThatAffectThisPylon->Empty();

	// build a list of mesh affecting pathobjects 	
	for(INT PathObjectIdx=0;PathObjectIdx<PathObjects.Num();++PathObjectIdx)
	{
		IInterface_NavMeshPathObject* CurrentPO = PathObjects(PathObjectIdx);
		if(CurrentPO->NeedsForbidExploreCheck(this))
		{
			PathObjectsThatAffectThisPylon->AddItem(CurrentPO);
		}
	}

	ForceUpdateComponents();

	{
		SCOPE_QUICK_TIMERX(Exploration,TRUE)
		GWarn->StatusUpdatef( CurrentPylonIndex, TotalNumOfPylons, TEXT("Exploring"));
		Explore_CreateGraph( Scout, Location );
		if( NavMeshPtr->Verts.Num() > MAXVERTID )
		{
			return FALSE;
		}
	}

	while(WorkingSetPtr->Num() > 0)
	{
		// pop head 
		FNavMeshPolyBase* CurNode = WorkingSetPtr->GetHead()->GetValue();
		WorkingSetPtr->RemoveNode(WorkingSetPtr->GetHead());
		DrawDebugCoordinateSystem(CurNode->GetPolyCenter(),FRotator(0,0,0),50.f,TRUE);
	}

	delete WorkingSetPtr;
	WorkingSetPtr = NULL;

	PathObjectsThatAffectThisPylon->Empty();
	delete PathObjectsThatAffectThisPylon;
	PathObjectsThatAffectThisPylon = NULL;

	return TRUE;
}

UBOOL APylon::NavMeshPass_ExpandSeeds()
{
	AWorldInfo *Info = GWorld->GetWorldInfo();

	// allow pathobjects to add seeds to exploration
	for(INT PathObjectIdx=0;PathObjectIdx<PathObjects.Num();++PathObjectIdx)
	{
		IInterface_NavMeshPathObject* CurrentPO = PathObjects(PathObjectIdx);
		CurrentPO->AddAuxSeedPoints(this);
	}

	UBOOL bDoPass = TRUE;
	while( bDoPass )
	{
		bDoPass = FALSE;

		// list of cover slots that add new seeds to the mesh
		TArray<FCoverInfo> Affectors;
		AScout* Scout = FPathBuilder::GetScout();
		GatherCoverReferences( Scout, Affectors );
		for( INT AffectorIdx=0;AffectorIdx<Affectors.Num();AffectorIdx++ )
		{
			ACoverLink* Link = Affectors(AffectorIdx).Link;
			INT SlotIdx = Affectors(AffectorIdx).SlotIdx;
			Link->BuildSlotInfo( SlotIdx, TRUE, Scout );
		}

		WorkingSetPtr = new TDoubleLinkedList<FNavMeshPolyBase*>();

		while( NextPassSeedList.Num() )
		{
			FVector SeedLoc = NextPassSeedList(0);
			NextPassSeedList.Remove( 0 );

			APylon* SeedPylon = NULL;
			FNavMeshPolyBase* SeedPoly = NULL;
			if( !UNavigationHandle::GetPylonAndPolyFromPos( SeedLoc,NAVMESHGEN_MIN_WALKABLE_Z, SeedPylon, SeedPoly ) )
			{
				SCOPE_QUICK_TIMERX(ExpandSeed,TRUE)
				GWarn->StatusUpdatef( CurrentPylonIndex, TotalNumOfPylons, TEXT("Exploring seeds"));
				if( Explore_CreateGraph( FPathBuilder::GetScout(), SeedLoc ) )
				{
					bDoPass = TRUE;
				}

				if( NavMeshPtr->Verts.Num() > MAXVERTID || GEngine->GetMapBuildCancelled() )
				{
					return FALSE;
				}
			}
		}

		while(WorkingSetPtr->Num() > 0)
		{
			// pop head 
			FNavMeshPolyBase* CurNode = WorkingSetPtr->GetHead()->GetValue();
			WorkingSetPtr->RemoveNode(WorkingSetPtr->GetHead());
			DrawDebugCoordinateSystem(CurNode->GetPolyCenter(),FRotator(0,0,0),50.f,TRUE);
		}

		delete WorkingSetPtr;
		WorkingSetPtr = NULL;
	}

	return TRUE;
}


UBOOL APylon::NavMeshPass_CreateEdgeConnections()
{
	if( DO_EDGE_CREATION )
	{
		NavMeshPtr->CreateEdgeConnections();
		NavMeshPtr->FixupForSaving();
	} 

	return TRUE;
}

UBOOL APylon::NavMeshPass_SimplifyMesh()
{

	if( !DO_SIMPLIFICATION )
	{
		return TRUE;
	}

	INT OldPolyCount = NavMeshPtr->BuildPolys.Num();
	GWarn->StatusUpdatef( CurrentPylonIndex, TotalNumOfPylons, TEXT("Simplifying mesh"));
	SCOPE_QUICK_TIMERX(Simplification,FALSE)
	NavMeshPtr->SimplifyMesh();
	if(ExpansionCullPolys)
	{
		NavMeshPtr->CullSillyPolys();

		// try again to simplify now that silliness is gone
		NavMeshPtr->SimplifyMesh(TRUE);
	}

	debugf(TEXT("Simplified mesh down to %i polys from %i"),NavMeshPtr->BuildPolys.Num(),OldPolyCount);
	return TRUE;
}

#if WITH_RECAST

/**
 * 2D filter for removing voxels outside allowed generation boundaries
 * (meshes are exported without any trimming)
 * @param x - X coord of tested column
 * @param y - Y coord of tested column
 * @param minh - bottom Z coord of tested column
 * @param maxh - top Z coord of tested column
 * @param context - pointer to pylon
 */
bool VoxelBoundsFilter(const float x, const float y, const float minh, const float maxh, void* context)
{
	APylon* GeneratingPylon = (APylon*)context;
	FVector TestLocation(x, y, (minh + maxh) * 0.5f);
	FVector TestExtent(1, 1, maxh - (minh + maxh) * 0.5f);

	for (INT iBounds = 0; iBounds < GeneratingPylon->VoxelFilterBounds.Num(); iBounds++)
	{
		FCheckResult HitTest(1.0f);
		FKAggregateGeom& Geom = GeneratingPylon->VoxelFilterBounds(iBounds);
		UBOOL bMiss = Geom.PointCheck(HitTest, GeneratingPylon->VoxelFilterTM(iBounds), FVector(1.0f), TestLocation, TestExtent);

		if (!bMiss)
		{
			return true;
		}
	}

	return false;
}

/**
 * Check if there is blocking collision between two points
 * @param FromLocation - first point
 * @param ToLocation - second point
 * @return TRUE if is blocked
 */
UBOOL IsSnapBlocked(const FVector& FromLocation, const FVector& ToLocation)
{
	const FVector StepOffset(0,0,FPathBuilder::GetScout()->NavMeshGen_MaxStepHeight);
	FVector TestPoint = (FromLocation + ToLocation) * 0.5f + StepOffset;

	// use unreal trace, snap target might be outside voxelized area :(
	FMemMark MemMark(GMainThreadMemStack);
	FCheckResult* Hits = GWorld->MultiPointCheck(GMainThreadMemStack,TestPoint,FVector(0),EXPANSION_TRACE_FLAGS);

	UBOOL bHit = FALSE;
	for (FCheckResult* Hit = Hits; Hit; Hit = Hit->GetNext())
	{
		if (Hit->Actor != NULL &&
			((Hit->Actor->bCollideActors && Hit->Actor->bBlockActors) || Hit->Actor->bWorldGeometry) &&
			(Hit->Component == NULL || Hit->Component->BlockNonZeroExtent) &&  Hit->Actor->bPathColliding &&
			!Hit->Actor->IsA(AInterpActor::StaticClass()))
		{
			bHit = TRUE;
			break;
		}
	}

	MemMark.Pop();
	return bHit;
}

/**
 * Check if point is inside generation bounds: use volumes if possible
 * @param TestPoint - point to be checked
 * @param SimpleBounds - pylon's extension bounds: box
 * @param LimitVolumes - pylon's extension bounds: volumes
 * @return TRUE is inside
 */
UBOOL IsPointInside(const FVector& TestPoint, const FBox& SimpleBounds, const TArray<AVolume*>& LimitVolumes)
{
	if (LimitVolumes.Num() > 0)
	{
		for (INT i = 0; i < LimitVolumes.Num(); i++)
		{
			if (LimitVolumes(i)->Encompasses(TestPoint))
			{
				return TRUE;
			}
		}

		return FALSE;
	}

	return SimpleBounds.IsInside(TestPoint);
}

/**
 * Exports geometry to OBJ file. Can be used to verify NavMesh generation in RecastDemo app
 * @param FileName - full name of OBJ file with extension
 * @param GeomVerts - list of vertices
 * @param GeomFaces - list of triangles (3 vert indices for each)
 */
void GeometryExportHelper(const FString& FileName, const TArray<FVector>& GeomVerts, const TArray<INT>& GeomFaces)
{
#if ALLOW_DEBUG_FILES
	FArchive* FileAr = GFileManager->CreateDebugFileWriter(*FileName);
	if (FileAr != NULL)
	{
		FStringOutputDevice Ar;
		for (INT i = 0; i < GeomVerts.Num(); i++)
		{
			const FLOAT VertScale = 0.01f;
			Ar.Logf(TEXT("v %f %f %f\n"), GeomVerts(i).X * VertScale, GeomVerts(i).Y * VertScale, GeomVerts(i).Z * VertScale);
		}

		for (INT i = 0; i < GeomFaces.Num(); i += 3)
		{
			Ar.Logf(TEXT("f %d %d %d\n"), GeomFaces(i + 0) +1, GeomFaces(i + 1) +1, GeomFaces(i + 2) +1);
		}

		FileAr->Logf(*Ar);
		FileAr->Close();
	}
#endif
}

#endif // WITH_RECAST

UBOOL APylon::NavMeshPass_Recast()
{
	UBOOL bResult = FALSE;

#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		GWarn->StatusUpdatef( CurrentPylonIndex, TotalNumOfPylons, TEXT("Building NavMesh (recast)"));
		SCOPE_QUICK_TIMERX(Recast,TRUE)

		NavMeshPtr = Cast<UNavigationMeshBase>(StaticConstructObject(UNavigationMeshBase::StaticClass(),this));
		NavMeshPtr->InitTransform(this);
		ObstacleMesh = Cast<UNavigationMeshBase>(StaticConstructObject(UNavigationMeshBase::StaticClass(),this));
		ObstacleMesh->InitTransform(this);

		// Gather path colliding geometry
		TArray<FVector> GeomVerts;
		TArray<INT> GeomFaces;

		IInterface_PylonGeometryProvider* GeomInt = InterfaceCast<IInterface_PylonGeometryProvider>(GEngine);
		if (GeomInt != NULL)
		{
			GeomInt->GetPathCollidingGeometry(this, GeomVerts, GeomFaces);

			// DEBUG: export geometry to OBJ file
			// GeometryExportHelper(FString::Printf(TEXT("%s%s_%s.obj"), *appGameLogDir(), *Pylon->GetOutermost()->GetName(), *Pylon->GetName()), GeomVerts, GeomFaces);
		}

		if (GeomVerts.Num() <= 0)
		{
			return TRUE;
		}

		NavMeshPass_Recast_MarkAlreadyBuiltAreas();
		NavMeshPass_Recast_MarkWalkableSeeds();
		NavMeshPass_Recast_SetupFilters();
		bResult = NavMeshPass_Recast_GenerateAndImport(GeomVerts, GeomFaces);
	}
#endif // WITH_RECAST

	return bResult; 
}

// NavMeshPass_Recast internal: marking areas with existing navmesh polys
void APylon::NavMeshPass_Recast_MarkAlreadyBuiltAreas()
{
#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		const FBox PylonBounds = GetBounds(WORLD_SPACE);
		const FLOAT hOffset = NAVMESHGEN_ENTITY_HALF_HEIGHT;
		TArray<APylon*> Pylons;
		TArray<rcConvexArea> ExcludedAreas;

		// Find already built pylons and add intersections as excluded area.
		UNavigationHandle::GetIntersectingPylons(PylonBounds.GetCenter(), PylonBounds.GetExtent(),Pylons);
		for (INT iPylon = 0; iPylon < Pylons.Num(); iPylon++)
		{
			APylon* IntersectingPylon = Pylons(iPylon);
			if (IntersectingPylon != this)
			{
				TArray<FNavMeshPolyBase*> Polys;
				IntersectingPylon->GetIntersectingPolys(PylonBounds.GetCenter(), PylonBounds.GetExtent()+FVector(NAVMESHGEN_STEP_SIZE), Polys, TRUE);
				for (INT PolyIdx = 0; PolyIdx < Polys.Num(); PolyIdx++)
				{
					FNavMeshPolyBase* Poly = Polys(PolyIdx);

					TArray<FVector> PolyVerts;
					for (INT iVert = 0; iVert < Poly->PolyVerts.Num(); iVert++)
					{
						PolyVerts.AddItem(Poly->GetVertLocation(iVert, TRUE));
					}

					if (PolyVerts.Num() > 0)
					{
						ExcludedAreas.AddZeroed();
						rcConvexArea& NewArea = ExcludedAreas(ExcludedAreas.Num() - 1);

						rcBuildConvexArea(&(PolyVerts.GetTypedData()->X), PolyVerts.Num(), NewArea);
						NewArea.minh -= hOffset;
						NewArea.maxh += hOffset;
					}
				}
			}
		}

		rcAddExcludedAreas(ExcludedAreas.GetTypedData(), ExcludedAreas.Num());
	}
#endif // WITH_RECAST
}

// NavMeshPass_Recast internal: marking walkable seeds for culling inaccessible polys
void APylon::NavMeshPass_Recast_MarkWalkableSeeds()
{
#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		// Find all covers within pylon's bounds and use them as accessible area seeds
		TArray<FVector> WalkableSeeds;

		FBox SimpleBounds = FBox::BuildAABB(Location, DrawScale3D * DrawScale * ExpansionRadius);
		TArray<AVolume*> LimitVolumes;
		for (INT i = 0; i < ExpansionVolumes.Num(); i++)
		{
			if (ExpansionVolumes(i) != NULL)
			{
				LimitVolumes.AddUniqueItem(ExpansionVolumes(i));
			}
		}

		for (ANavigationPoint* iNav = GWorld->GetFirstNavigationPoint(); iNav != NULL; iNav = iNav->nextNavigationPoint)
		{
			ACoverLink* CovLink = Cast<ACoverLink>(iNav);
			if (CovLink != NULL && !CovLink->bAlreadyVisited && IsPointInside(CovLink->Location, SimpleBounds, LimitVolumes))
			{
				CovLink->bAlreadyVisited = TRUE;
				WalkableSeeds.AddItem(CovLink->Location);
			}
		}

		// Allow pathobjects to add seeds to exploration
		for (INT PathObjectIdx = 0; PathObjectIdx < PathObjects.Num(); PathObjectIdx++)
		{
			IInterface_NavMeshPathObject* CurrentPO = PathObjects(PathObjectIdx);
			CurrentPO->AddAuxSeedPoints(this);
		}

		// ... but limit them to unexplored part of map
		for (INT i = 0; i < NextPassSeedList.Num(); i++)
		{
			FVector SeedLoc = NextPassSeedList(i);
			APylon* SeedPylon = NULL;
			FNavMeshPolyBase* SeedPoly = NULL;
			if (IsPointInside(SeedLoc, SimpleBounds, LimitVolumes) &&
				!UNavigationHandle::GetPylonAndPolyFromPos(SeedLoc, NAVMESHGEN_MIN_WALKABLE_Z, SeedPylon, SeedPoly))
			{
				WalkableSeeds.AddItem(SeedLoc);
			}
		}
		NextPassSeedList.Empty();

		// Add pylon's location as accessible area seed
		WalkableSeeds.AddItem(Location);

		FVector PolySearchExtent(150.0f, 150.0f, 800.0f);
		rcAddWalkableSeeds(&WalkableSeeds(0).X, WalkableSeeds.Num(), &PolySearchExtent.X);
	}
#endif // WITH_RECAST
}

// NavMeshPass_Recast internal: prepare voxel filter data
void APylon::NavMeshPass_Recast_SetupFilters()
{
#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		VoxelFilterBounds.Empty();
		VoxelFilterTM.Empty();
		for (INT i = 0; i < ExpansionVolumes.Num(); i++)
		{
			AVolume* BoundVolume = ExpansionVolumes(i);
			if (BoundVolume != NULL && BoundVolume->BrushComponent != NULL)
			{
				VoxelFilterBounds.AddItem(BoundVolume->BrushComponent->BrushAggGeom);
				VoxelFilterTM.AddItem(BoundVolume->LocalToWorld());
			}
		}

		if (VoxelFilterBounds.Num() == 0)
		{
			FKAggregateGeom SimpleBounds;
			appMemzero(&SimpleBounds, sizeof(SimpleBounds));

			FKBoxElem SimpleBox(ExpansionRadius * 2);
			SimpleBox.TM = FMatrix::Identity;
			SimpleBounds.BoxElems.AddItem(SimpleBox);
			VoxelFilterBounds.AddItem(SimpleBounds);

			FMatrix SimpleTM = FScaleMatrix(DrawScale3D * DrawScale) * FTranslationMatrix(Location);
			VoxelFilterTM.AddItem(SimpleTM);
		}
	}
#endif // WITH_RECAST
}

/** 
 * NavMeshPass_Recast internal: generating navmesh and importing its data
 * @param GeomVerts - verts of collision geometry
 * @param GeomFaces - list of vert indices for collision geometry triangles (3 entries for each)
 */
UBOOL APylon::NavMeshPass_Recast_GenerateAndImport(const TArray<FVector>& GeomVerts, const TArray<INT>& GeomFaces)
{
#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		// Setup recast parameters: common for every pylon - makes snapping easier (matching coords in voxel grid)
		AScout* Scout = FPathBuilder::GetScout();
		const FLOAT CellSize = 25.0;
		const FLOAT CellHeight = 10.0;
		const FLOAT AgentHeight = Scout->NavMeshGen_EntityHalfHeight * 2.0f;
		const FLOAT MaxAgentHeight = NAVMESHGEN_MAX_POLY_HEIGHT_PYLON(this);
		const FLOAT AgentMaxSlope = 45.0;
		const FLOAT AgentMaxClimb = Scout->NavMeshGen_MaxStepHeight;

		// used for lowering bounds of Recast generated poly: at least AgentMaxClimb
		RecastBoundsZOffset = AgentMaxClimb * 2.0f;
		// used for snapping nearby vertices to other vertices: at least 1.5x CellSize
		RecastSnapExtent = FVector(CellSize * 2.0f);
		// used for snapping nearby vertices to edges: at least 3x CellSize
		RecastEdgeSnapExtent = FVector(CellSize * 3.2f);
	
		rcSetupGeneration(CellSize, CellHeight, AgentHeight, MaxAgentHeight, AgentMaxSlope, AgentMaxClimb);

		// Generate navmesh
		float* NavMeshVerts = NULL;
		int nNavMeshVerts = 0;
		rcNavMeshPoly* NavMeshPolys = NULL;
		int nNavMeshPolys = 0;
		float* DropDownVerts = NULL;
		int nDropDownVerts = 0;

		UBOOL bSuccess = rcGenerateNavMesh(
			(float*)GeomVerts.GetTypedData(), GeomVerts.Num(), GeomFaces.GetTypedData(), GeomFaces.Num() / 3,
			VoxelBoundsFilter, this,
			NavMeshVerts, nNavMeshVerts, NavMeshPolys, nNavMeshPolys, DropDownVerts, nDropDownVerts);

		if (bSuccess)
		{
			// convert navmesh data into FMeshVertex & FNavMeshPolyBase:

			// - convert verts
			TArray<FMeshVertex> ImportVerts;
			for (INT i = 0; i < nNavMeshVerts; i++)
			{
				FVector WorldVertItem(NavMeshVerts[i * 3 + 0], NavMeshVerts[i * 3 + 1], NavMeshVerts[i * 3 + 2]);
				FVector LocalVertItem = NavMeshPtr->W2LTransformFVector(WorldVertItem);

				ImportVerts.AddItem(FMeshVertex(LocalVertItem));
			}

			delete[] NavMeshVerts;
			NavMeshVerts = NULL;

			// - convert polys
			TArray<FNavMeshPolyBase*> ImportPolys;
			TArray<BYTE> BorderPolys;
			for (INT i = 0; i < nNavMeshPolys; i++)
			{
				FNavMeshPolyBase* PtrPolyItem = new FNavMeshPolyBase();
				appMemzero(PtrPolyItem, sizeof(FNavMeshPolyBase));

				PtrPolyItem->PolyHeight = NavMeshPolys[i].height;
				for (INT iv = 0; iv < MAX_VERTS_PER_POLY; iv++)
				{
					if (NavMeshPolys[i].verts[iv] < 0) break;
					PtrPolyItem->PolyVerts.AddItem(NavMeshPolys[i].verts[iv]);
				}

				BorderPolys.AddItem(NavMeshPolys[i].isBorderPoly ? 1 : 0);
				ImportPolys.AddItem(PtrPolyItem);
			}

			delete[] NavMeshPolys;
			NavMeshPolys = NULL;

			// create build polys from converted data
			NavMeshPtr->ImportBuildPolys(ImportVerts, ImportPolys, BorderPolys, RecastBoundsZOffset);

			// use drop down data to create special polys
			// every entry has 4 values: 012 = XYZ, 3=drop height
			for (INT i = 0; i < nDropDownVerts; i++)
			{
				FLOAT DropHeight = DropDownVerts[i * 4 + 3];
				if (DropHeight <= Scout->NavMeshGen_MaxDropHeight)
				{
					FVector DropFromLoc(DropDownVerts[i * 4 + 0], DropDownVerts[i * 4 + 1], DropDownVerts[i * 4 + 2]);
					FVector DropToLoc = DropFromLoc - FVector(0,0,DropHeight);

					SavePossibleDropDownEdge(DropToLoc, DropFromLoc, FVector(0,0,1), NULL, TRUE);
				}
			}

			delete[] DropDownVerts;
			DropDownVerts = NULL;

			return TRUE;
		}
	}
#endif // WITH_RECAST

	return FALSE;
}


UBOOL APylon::NavMesh_MungeVerts()
{
	return NavMesh_MungeVertsInternal();
}


#if WITH_RECAST

/**
 * Fix NavMesh after moving verts (poly can end up with more than one vertex in the same location)
 * @param NavMesh - NavMesh to fix
 */
void FixNavMeshAfterSnap(UNavigationMeshBase* NavMesh)
{
	TArray<FNavMeshPolyBase*> PolysToRemove;

	for (PolyList::TIterator Itt(NavMesh->BuildPolys.GetTail()); Itt; --Itt)
	{
		FNavMeshPolyBase* CurPoly = *Itt;

		// bubble through and remove duplicated vertices
		for (INT i1 = CurPoly->PolyVerts.Num() - 1; i1 >= 0; i1--)
		{
			FMeshVertex& TestVert1 = NavMesh->Verts(CurPoly->PolyVerts(i1));
			for (INT i2 = 0; i2 < i1; i2++)
			{
				FMeshVertex& TestVert2 = NavMesh->Verts(CurPoly->PolyVerts(i2));
				if (TestVert1 == TestVert2)
				{
					CurPoly->PolyVerts.Remove(i1);
					// clear poly reference in vertex removed from PolyVerts
					TestVert1.ContainingPolys.RemoveItem(CurPoly);
					// make sure that other vertex contains poly reference (e.g. duplicated indices in PolyVerts)
					TestVert2.ContainingPolys.AddUniqueItem(CurPoly);
					break;
				}
			}
		}

		// remove degenerated polys (less than 3 unique verts)
		//@note: needs to be deferred so we don't modify the BuildPolys list while we're iterating it
		UBOOL bRemove = (CurPoly->PolyVerts.Num() < 3 || CurPoly->CalcArea() < NAVMESHGEN_MIN_POLY_AREA);
		if (bRemove)
		{			
			PolysToRemove.AddItem(CurPoly);
		}
	}
	for (INT i = 0; i < PolysToRemove.Num(); i++)
	{
		NavMesh->RemovePoly(PolysToRemove(i));
	}
}

/**
 * Split poly into smaller, convex ones, skipping over parts degenerated to line due to aggresive snapping.
 * @param Verts - list of vertices (world space)
 * @param SrcPoly - polygon to split
 * @param OutsidePoly - if set, polys will be created only if OutsidePoly doesn't contain their center
 * @return TRUE if SrcPoly is degenerated
 */
UBOOL SplitDegeneratedPoly(const TArray<FVector>& Verts, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* OutsidePoly=NULL)
{
	// try to flattern poly if Z different between vertices is insignificant, and reject entire poly if it's a line (area < threshold)
	if (Verts.Num() >= 3)
	{
		TArray<FVector> FlatVerts = Verts;
		UBOOL bCanTestFlatVerts = TRUE;
		FLOAT BaseZ = FlatVerts(0).Z;
		for (INT i = 1; i < FlatVerts.Num(); i++)
		{
			if (Abs(FlatVerts(i).Z - BaseZ) < 5.0f)
			{
				FlatVerts(i).Z = BaseZ;
			}
			else
			{
				bCanTestFlatVerts = FALSE;
				break;
			}
		}

		if (bCanTestFlatVerts)
		{
			FLOAT FlatArea = FNavMeshPolyBase::CalcArea(FlatVerts);
			if (FlatArea < NAVMESHGEN_MIN_POLY_AREA)
			{
				return TRUE;
			}
		}
	}

	static TArray<INT> DegeneratedVertsIdx;
	static TArray<INT> DegeneratedVertsEdge;
	DegeneratedVertsIdx.Reset();
	DegeneratedVertsEdge.Reset();

	// find all vertices on edges of the same poly
	for (INT iVert = 0; iVert < Verts.Num(); iVert++)
	{
		for (INT iEdge = 0; iEdge < Verts.Num(); iEdge++)
		{
			const INT PrevVert = (iVert + Verts.Num() - 1) % Verts.Num();
			if (iEdge == iVert || iEdge == PrevVert) continue;

			const INT iEdgeEnd = (iEdge + 1) % Verts.Num();
			FVector DummyPt(0);
			FLOAT Dist = PointDistToSegment(Verts(iVert), Verts(iEdgeEnd), Verts(iEdge), DummyPt);
			if (Dist < 5.0f)
			{
				DegeneratedVertsIdx.AddItem(iVert);
				DegeneratedVertsEdge.AddItem(iEdge);
			}
		}
	}

	if (DegeneratedVertsIdx.Num() <= 0)
		return FALSE;

	// split into new polys using degenerated data
	UBOOL bLastVertexProcessed = FALSE;
	FVector NewPolyCenter(0.0f);
	INT QuitIdx = INDEX_NONE;
	static TArray<FVector> NewPolyVerts;
	NewPolyVerts.Reset();

	for (INT Idx = 0;; Idx = (Idx + 1) % Verts.Num())
	{
		NewPolyVerts.AddItem(Verts(Idx));
		NewPolyCenter += Verts(Idx);

		if (Idx == Verts.Num() - 1)
		{
			bLastVertexProcessed = TRUE;
		}

		INT DegeneratedIdx = DegeneratedVertsIdx.FindItemIndex(Idx);
		if (DegeneratedIdx != INDEX_NONE)
		{
			QuitIdx = DegeneratedVertsEdge(DegeneratedIdx);
		}

		if (Idx == QuitIdx ||
			(DegeneratedIdx != INDEX_NONE && NewPolyVerts.Num() > 1))
		{
			// try to add new poly
			if (NewPolyVerts.Num() > 2)
			{
				UBOOL bCanAdd = TRUE;
				if (OutsidePoly != NULL)
				{
					NewPolyCenter /= NewPolyVerts.Num();
					bCanAdd = !OutsidePoly->ContainsPoint(NewPolyCenter);
				}

				if (bCanAdd)
				{
					SrcPoly->NavMesh->AddPoly(NewPolyVerts, SrcPoly->PolyHeight);
				}
			}

			NewPolyVerts.Reset();
			QuitIdx = INDEX_NONE;
			NewPolyCenter = FVector(0.f);

			if (bLastVertexProcessed) break;
		}
	}

	return TRUE;
}

/**
 * Checks if list contain given point with some small threshold
 * @param Verts - list of points
 * @param TestVert - point to test
 * @return TRUE if point is already on the list
 */
UBOOL ContainsSimilairPoint(const TArray<FVector>& Verts, FVector TestVert)
{
	for (INT i = 0; i < Verts.Num(); i++)
	{
		if ((Verts(i) - TestVert).SizeSquared() < 1.0)
		{
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Checks if list of vertices will create the same poly as passed to function
 * @param Poly - poly to compare
 * @param TestVertsWS - list of vertex locations (world space)
 * @return TRUE if vertices are the same as poly
 */
UBOOL IsNearlyEqualPoly(FNavMeshPolyBase* Poly, const TArray<FVector>& TestVertsWS)
{
	if (Poly->PolyVerts.Num() != TestVertsWS.Num())
		return FALSE;

	for (INT i = 0; i < Poly->PolyVerts.Num(); i++)
	{
		VERTID PolyVertId = Poly->PolyVerts(i);
		VERTID NewVertId = Poly->NavMesh->FindVert(TestVertsWS(i), WORLD_SPACE);
		if (NewVertId != PolyVertId)
			return FALSE;
	}

	return TRUE;
}

/**
 * Simple test if two polys are adjacent - share one edge and don't really overlap each other
 * @param Poly1WS - vertices of first poly (world space)
 * @param Poly2WS - vertices of second poly (world space)
 * @return TRUE if polys are adjacent
 */
UBOOL IsAdjacentPoly(const TArray<FVector>& Poly1WS, const TArray<FVector>& Poly2WS)
{
	// polys are adjacent if they share single edge and all other verts of second poly are outside first poly
	INT SkipPoly2V0 = INDEX_NONE;
	INT SkipPoly2V1 = INDEX_NONE;

	UBOOL bFoundEdge = FALSE;
	for (INT i1 = 0; i1 < Poly1WS.Num(); i1++)
	{
		INT i2 = Poly2WS.FindItemIndex(Poly1WS(i1));
		if (i2 != INDEX_NONE)
		{
			INT i1Next = (i1 + 1) % Poly1WS.Num();
			INT i1Prev = (i1 + Poly1WS.Num() - 1) % Poly1WS.Num();
			INT i2Next = (i2 + 1) % Poly2WS.Num();

			SkipPoly2V0 = i2;
			SkipPoly2V1 = i2Next;
			
			if (Poly2WS(i2Next) == Poly1WS(i1Next) || Poly2WS(i2Next) == Poly1WS(i1Prev))
			{
				bFoundEdge = TRUE;
				break;
			}
		}
	}

	if (!bFoundEdge)
		return FALSE;

	for (INT i2 = 0; i2 < Poly2WS.Num(); i2++)
	{
		if (i2 != SkipPoly2V0 && i2 != SkipPoly2V1)
		{
			if (FNavMeshPolyBase::ContainsPoint(Poly1WS, Poly2WS(i2)))
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/**
 * Simple test if vertex belongs to poly on NavMesh border
 * (will return TRUE for some verts inside NavMesh as well, but it doesn't really matter)
 * @param NavMesh - NavMesh owning vertex for testing
 * @param Idx - vertex index for testing
 * @return TRUE if vertex is on border (or next to border)
 */
UBOOL IsBorderVertex(UNavigationMeshBase* NavMesh, VERTID Idx)
{
	for (INT i = 0; i < NavMesh->Verts(Idx).ContainingPolys.Num(); i++)
	{
		FNavMeshPolyBase* TestPoly = NavMesh->Verts(Idx).ContainingPolys(i);
		if (NavMesh->BorderPolys.FindNode(TestPoly) == NULL)
		{
			return FALSE;
		}
	}

	return TRUE;
}


#endif // WITH_RECAST

const FLOAT VertCompRange = 5.0f;
const FLOAT EdgeSnapDist = 30.0f;

UBOOL APylon::NavMeshPass_RecastSnap()
{
#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		TArray<INT> VertsForEdgeSnap;
		TArray<FNavMeshPolyBase*> ModifiedPolys;
	
		NavMeshPass_RecastSnap_VertToOtherVert(VertsForEdgeSnap);
		FixNavMeshAfterSnap(NavMeshPtr);

		NavMeshPass_RecastSnap_OtherVertToEdge(ModifiedPolys);
		FixNavMeshAfterSnap(NavMeshPtr);

		NavMeshPass_RecastSnap_VertToOtherEdge(VertsForEdgeSnap);	
		FixNavMeshAfterSnap(NavMeshPtr);

		NavMeshPass_RecastSnap_CutOverlapping(ModifiedPolys);
	}
#endif // WITH_RECAST

	return TRUE;
}

/** 
 * NavMeshPass_RecastSnap internal: snapping verts of this navmesh to verts of already built navmeshes
 * @param UnprocessedVerts - indices of verts that were not snapped in this step
 */
void APylon::NavMeshPass_RecastSnap_VertToOtherVert(TArray<INT>& UnprocessedVerts)
{
#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		// snap list of vertices to already built navmesh polys
		// unless there is a blocking collision between them

		for (INT i = 0; i < NavMeshPtr->Verts.Num(); i++)
		{
			FMeshVertex& SnapVert = NavMeshPtr->Verts(i);
			if (SnapVert.ContainingPolys.Num() <= 0 || !SnapVert.IsBorderVert(i))
				continue;

			FVector SnapVertLoc = NavMeshPtr->GetVertLocation(i, WORLD_SPACE);
			TArray<FNavMeshPolyBase*> Polys;
			UNavigationHandle::GetAllPolysFromPos(SnapVertLoc, RecastSnapExtent, Polys, TRUE);

			FVector BestMatch;
			FLOAT BestScore = -1.0;

			for (INT iPoly = 0; iPoly < Polys.Num(); iPoly++)
			{
				FNavMeshPolyBase* PolyItem = Polys(iPoly);
				APylon* PolyPylon = PolyItem->NavMesh->GetPylon();

				// recast generated mesh - snap to all other polys
				// native generated mesh - snap to recast polys
				if (PolyItem->NavMesh != NavMeshPtr &&
					((PolyPylon != NULL && PolyPylon->NavMeshGenerator == NavMeshGenerator_Recast) || NavMeshGenerator == NavMeshGenerator_Recast))
				{
					for (INT iVert = 0; iVert < PolyItem->PolyVerts.Num(); iVert++)
					{
						FVector PolyVert = PolyItem->GetVertLocation(iVert, WORLD_SPACE);
						FVector Diff = PolyVert - SnapVertLoc;

						if (Abs(Diff.X) <= RecastSnapExtent.X && 
							Abs(Diff.Y) <= RecastSnapExtent.Y &&
							Abs(Diff.Z) <= RecastSnapExtent.Z &&
							!IsSnapBlocked(PolyVert, SnapVertLoc))
						{
							// if snapping to vertex inside other mesh, find nearest edge and move point on to it
							if (SnapVert.ContainingPolys.Num() > 0)
							{
								const FVector TestPt = PolyVert + (SnapVertLoc - PolyVert).SafeNormal() * 5.0f;
								TArray<FNavMeshPolyBase*> OverlappingPolys;
								UNavigationHandle::GetAllPolysFromPos(TestPt, FVector(1,1,RecastSnapExtent.Z), OverlappingPolys, TRUE);

								FLOAT BestEdgeScore = -1;
								FVector PointOnEdge;

								for (INT iOverlapping = 0; iOverlapping < OverlappingPolys.Num(); iOverlapping++)
								{
									FNavMeshPolyBase* OverlappingPoly = OverlappingPolys(iOverlapping);
									if (OverlappingPoly->NavMesh == NavMeshPtr || !OverlappingPoly->ContainsPoint(TestPt, WORLD_SPACE))
										continue;

									// best edge = closest to original location
									FVector V0 = OverlappingPoly->GetVertLocation(0, WORLD_SPACE);
									for (INT iEdge = 1; iEdge <= OverlappingPoly->PolyVerts.Num(); iEdge++)
									{
										FVector V1 = OverlappingPoly->GetVertLocation(iEdge % OverlappingPoly->PolyVerts.Num(), WORLD_SPACE);
										FVector PolyCenterOnEdge;
										FLOAT EdgeScore = PointDistToSegment(SnapVertLoc, V0, V1, PolyCenterOnEdge);
										if (BestEdgeScore < 0 || BestEdgeScore > EdgeScore)
										{
											PointDistToSegment(SnapVert, V0, V1, PointOnEdge);
											BestEdgeScore = EdgeScore;
										}

										V0 = V1;
									}
								}

								if (BestEdgeScore >= 0)
								{
									PolyVert = PointOnEdge;
									Diff = PolyVert - SnapVertLoc;
								}
							}

							FLOAT VertScore = Diff.SizeSquared();
							if (BestScore < 0 || BestScore > VertScore)
							{
								BestScore = VertScore;
								BestMatch = PolyVert;
							}
						}
					}
				}
			}

			if (BestScore >= 0)
			{
				NavMeshPtr->MoveVert(i, BestMatch, WORLD_SPACE);
			}
			else
			{
				UnprocessedVerts.AddItem(i);
			}
		}
	}
#endif // WITH_RECAST
}

/** 
 * NavMeshPass_RecastSnap internal: snapping verts of already built navmeshes to edges of this navmesh
 * @param ModifiedPolys - list of modified polys on other navmeshes
 */
void APylon::NavMeshPass_RecastSnap_OtherVertToEdge(TArray<FNavMeshPolyBase*>& ModifiedPolys)
{
#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		//  find vertices of existing meshes close enough/overlapping currently build polys and move them to edges
		for (PolyList::TIterator Itt(NavMeshPtr->BuildPolys.GetTail()); Itt; --Itt)
		{
			FNavMeshPolyBase* Poly = *Itt;
			TArray<FNavMeshPolyBase*> Polys;

			const FBox PolyBounds = Poly->GetPolyBounds(WORLD_SPACE);
			UNavigationHandle::GetAllPolysFromPos(PolyBounds.GetCenter(), PolyBounds.GetExtent() + FVector(EdgeSnapDist), Polys, TRUE);
			if (Polys.Num() <= 0)
				continue;

			for (INT iPoly = 0; iPoly < Polys.Num(); iPoly++)
			{
				FNavMeshPolyBase* CurPoly = Polys(iPoly);
				UNavigationMeshBase* OtherMesh = CurPoly->NavMesh;
				APylon* OtherPylon = (OtherMesh != NULL) ? OtherMesh->GetPylon() : NULL;

				// don't snap to poly on the same navmesh
				if (CurPoly->NavMesh == NavMeshPtr)
					continue;
				
				// don't snap when neither this pylon nor current intersecting one uses Recast
				if (NavMeshGenerator != NavMeshGenerator_Recast && (OtherPylon == NULL || OtherPylon->NavMeshGenerator != NavMeshGenerator_Recast))
					continue;

				for (INT iVert = 0; iVert < CurPoly->PolyVerts.Num(); iVert++)
				{
					FVector VertLoc = CurPoly->GetVertLocation(iVert, WORLD_SPACE);

					// find closest edge of build poly for every vertex of poly in snap range
					FLOAT BestScore = -1;
					FVector BestPointOnEdge;
					INT BestEdgeV0 = 0;
					UBOOL bAlreadySnapped = FALSE;

					FVector V0 = Poly->GetVertLocation(0, WORLD_SPACE);
					for (INT i = 1; i <= Poly->PolyVerts.Num(); i++)
					{
						FVector V1 = Poly->GetVertLocation(i % Poly->PolyVerts.Num(), WORLD_SPACE);
						FVector PointOnEdge;

						FLOAT DistFromEdge = PointDistToSegment(VertLoc, V0, V1, PointOnEdge);
						if (BestScore < 0 || BestScore > DistFromEdge)
						{
							BestScore = DistFromEdge;
							BestPointOnEdge = PointOnEdge;
							BestEdgeV0 = i - 1;
						}

						if (V0.Equals(VertLoc, VertCompRange))
						{
							bAlreadySnapped = TRUE;
							break;
						}

						V0 = V1;
					}

					// if edge is close enough, or point is within build poly, perform snap:
					//   move vertex of exiting mesh
					//   add a vertex to current build poly in the same location as moved one
					//   don't update VertsForEdgeSnap list, since new vertex is snapped to other mesh
					if (!bAlreadySnapped && (BestScore <= EdgeSnapDist || Poly->ContainsPoint(VertLoc, WORLD_SPACE)))
					{
						// existing navmesh (moving vertex will modify its hash, so keep it updated)
						VERTID ExistingVertId = CurPoly->PolyVerts(iVert);
						FMeshVertex& ExistingVert = OtherMesh->Verts(ExistingVertId);
						OtherMesh->MoveVert(ExistingVertId, BestPointOnEdge, WORLD_SPACE);
					
						const FLOAT BoundsZOffset = (OtherPylon->NavMeshGenerator == NavMeshGenerator_Recast) ? RecastBoundsZOffset : 0.0f;
						for (INT iRecalcPoly = 0; iRecalcPoly < ExistingVert.ContainingPolys.Num(); iRecalcPoly++)
						{
							FNavMeshPolyBase* RecalcPoly = ExistingVert.ContainingPolys(iRecalcPoly);
							FVector PolyUp(0,0,1);

							RecalcPoly->BoxBounds = FBox(0);
							RecalcPoly->RecalcAfterVertChange(&PolyUp);
							ModifiedPolys.AddUniqueItem(RecalcPoly);

							for (INT iRecalcVert = 0; iRecalcVert < RecalcPoly->PolyVerts.Num(); iRecalcVert++)
							{
								FVector RecalcVertLoc = RecalcPoly->GetVertLocation(iRecalcVert, LOCAL_SPACE);
								RecalcPoly->BoxBounds += RecalcVertLoc + PolyUp * RecalcPoly->PolyHeight;
								RecalcPoly->BoxBounds += RecalcVertLoc - PolyUp * BoundsZOffset;
							}
						}

						// current navmesh
						VERTID NewVertId = NavMeshPtr->AddVert(BestPointOnEdge, WORLD_SPACE);
						NavMeshPtr->Verts(NewVertId).ContainingPolys.AddUniqueItem(Poly);

						if (!Poly->PolyVerts.ContainsItem(NewVertId))
						{
							Poly->PolyVerts.InsertItem(NewVertId, BestEdgeV0 + 1);
						}
					}
				}
			}
		}
	}
#endif // WITH_RECAST
}

/** 
 * NavMeshPass_RecastSnap internal: snapping verts of this navmesh to edges of already build navmeshes
 * @param VertsToSnap - indices of verts to consider
 */
void APylon::NavMeshPass_RecastSnap_VertToOtherEdge(const TArray<INT>& VertsToSnap)
{
#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		for (INT i = 0; i < VertsToSnap.Num(); i++)
		{
			VERTID SnapVertId = VertsToSnap(i);
			FMeshVertex& SnapVert = NavMeshPtr->Verts(SnapVertId);
			FVector SnapVertLoc = NavMeshPtr->L2WTransformFVector(SnapVert);

			TArray<FNavMeshPolyBase*> Polys;
			UNavigationHandle::GetAllPolysFromPos(SnapVertLoc, RecastEdgeSnapExtent, Polys, TRUE);
			if (Polys.Num() <= 0)
				continue;

			UNavigationMeshBase::BorderEdgeInfo BestEdgeInfo;
			FNavMeshPolyBase* BestEdgePoly = NULL;
			FVector BestPointOnEdge;
			FLOAT BestScore = -1.0;

			// prepare list of verts connected to this one
			TArray<INT> ConnectedVerts;
			for (INT iConnected = 0; iConnected < SnapVert.ContainingPolys.Num(); iConnected++)
			{
				FNavMeshPolyBase* TestPoly = SnapVert.ContainingPolys(iConnected);
				INT LocalIdx = INDEX_NONE;
			
				TestPoly->PolyVerts.FindItem(VertsToSnap(i), LocalIdx);
				if (LocalIdx != INDEX_NONE)
				{
					ConnectedVerts.AddUniqueItem(TestPoly->GetAdjacentVertPoolIndex(LocalIdx, +1));
					ConnectedVerts.AddUniqueItem(TestPoly->GetAdjacentVertPoolIndex(LocalIdx, -1));
				}
			}

			// for every polygon in snap range, find best edge to snap to
			for (INT iPoly = 0; iPoly < Polys.Num(); iPoly++)
			{
				FNavMeshPolyBase* CurPoly = Polys(iPoly);
				UNavigationMeshBase* OtherMesh = CurPoly->NavMesh;
				APylon* OtherPylon = OtherMesh->GetPylon();

				// don't snap to poly on the same navmesh, or to owning poly
				if (OtherMesh == NavMeshPtr || CurPoly->PolyVerts.ContainsItem(VertsToSnap(i)))
					continue;

				// don't snap when neither this pylon nor current intersecting one uses Recast
				if (NavMeshGenerator != NavMeshGenerator_Recast && (OtherPylon == NULL || OtherPylon->NavMeshGenerator != NavMeshGenerator_Recast))
					continue;

				// check all border edges
				for (INT iEdge = 0; iEdge < CurPoly->PolyVerts.Num(); iEdge++)
				{
					const INT VertIdx0 = CurPoly->PolyVerts(iEdge);
					const INT VertIdx1 = CurPoly->PolyVerts((iEdge + 1) % CurPoly->PolyVerts.Num());
					if (!IsBorderVertex(OtherMesh, VertIdx0) || !IsBorderVertex(OtherMesh, VertIdx1))
					{
						continue;
					}

					FVector V0 = OtherMesh->GetVertLocation(VertIdx0, WORLD_SPACE);
					FVector V1 = OtherMesh->GetVertLocation(VertIdx1, WORLD_SPACE);

					// skip when too far away
					FVector PointOnEdge;
					FLOAT DistToEdge = PointDistToSegment(SnapVertLoc, V0, V1, PointOnEdge);
					if (DistToEdge > EdgeSnapDist)
						continue;

					// find up to 2 verts connected to current snap vert on local navmesh, in the same places as current edge verts
					VERTID ConnectedV0 = MAXVERTID;
					VERTID ConnectedV1 = MAXVERTID;
					for (INT iConnected = 0; iConnected < ConnectedVerts.Num(); iConnected++)
					{
						VERTID ConnectedId = ConnectedVerts(iConnected);
						FVector ConnectedVertLoc = NavMeshPtr->GetVertLocation(ConnectedId, WORLD_SPACE);
					
						if (V0.Equals(ConnectedVertLoc, VertCompRange))
						{
							ConnectedV0 = ConnectedId;
						}
						if (V1.Equals(ConnectedVertLoc, VertCompRange))
						{
							ConnectedV1 = ConnectedId;
						}
					}

					if (ConnectedV0 == MAXVERTID && ConnectedV1 == MAXVERTID)
						continue;

					if (BestScore < 0 || BestScore > DistToEdge)
					{
						BestScore = DistToEdge;
						BestEdgeInfo.Vert0 = VertIdx0;
						BestEdgeInfo.Vert1 = VertIdx1;
						BestEdgePoly = CurPoly;
						BestPointOnEdge = PointOnEdge;
					}
				}
			}

			// snap vert to best edge:
			//   insert new vertex to existing navmesh in snap location & update affected poly
			//   move snap vertex to new location & update affected poly
			if (BestScore >= 0)
			{
				// existing mesh
				UNavigationMeshBase* OtherMesh = BestEdgePoly->NavMesh;

				VERTID NewVertId = OtherMesh->AddVert(BestPointOnEdge, WORLD_SPACE);
				FMeshVertex& NewVert = OtherMesh->Verts(NewVertId);
				NewVert.ContainingPolys.AddUniqueItem(BestEdgePoly);

				if (!BestEdgePoly->PolyVerts.ContainsItem(NewVertId))
				{
					const INT LocalIdx0 = BestEdgePoly->PolyVerts.FindItemIndex(BestEdgeInfo.Vert0);
					BestEdgePoly->PolyVerts.InsertItem(NewVertId, LocalIdx0 + 1);
				}

				// current mesh
				NavMeshPtr->MoveVert(SnapVertId, BestPointOnEdge, WORLD_SPACE);
			}
		}
	}
#endif // WITH_RECAST
}

/** 
 * NavMeshPass_RecastSnap internal: cutting overlapping and degenerated polys
 * @param ModifiedPolys - list of modified polys on other navmeshes
 */
void APylon::NavMeshPass_RecastSnap_CutOverlapping(const TArray<FNavMeshPolyBase*>& ModifiedPolys)
{
#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		// remove degenerated polys
		TArray<FNavMeshPolyBase*> DegeneratedTestList = ModifiedPolys;
		for (PolyList::TIterator Itt(NavMeshPtr->BuildPolys.GetTail()); Itt; --Itt)
		{
			DegeneratedTestList.AddItem(*Itt);
		}

		for (INT iPoly = 0; iPoly < DegeneratedTestList.Num(); iPoly++)
		{
			FNavMeshPolyBase* Poly = DegeneratedTestList(iPoly);
		
			// cache verts locations in world space
			static TArray<FVector> PolyWS;
			PolyWS.Reset();
			for (INT iVert = 0; iVert < Poly->PolyVerts.Num(); iVert++)
			{
				PolyWS.AddItem(Poly->GetVertLocation(iVert, WORLD_SPACE));
			}

			// fix and remove degenerated ones
			UBOOL bIsDegenerated = SplitDegeneratedPoly(PolyWS, Poly);
			if (bIsDegenerated)
			{
				Poly->NavMesh->RemovePoly(Poly);
			}
		}

		// clip overlapping polys
		TArray<FNavMeshPolyBase*> PolysToRemove;
		for (PolyList::TIterator Itt(NavMeshPtr->BuildPolys.GetHead()); Itt; ++Itt)
		{
			FNavMeshPolyBase* Poly = *Itt;
			TArray<FNavMeshPolyBase*> Polys;

			const FBox PolyBounds = Poly->GetPolyBounds(WORLD_SPACE);
			UNavigationHandle::GetAllPolysFromPos(PolyBounds.GetCenter(), PolyBounds.GetExtent(), Polys, TRUE);
			if (Polys.Num() <= 0)
				continue;

			// cache verts locations in world space
			static TArray<FVector> Poly1WS;
			Poly1WS.Reset();
			FVector Poly1Center(0.f);
			for (INT iVert = 0; iVert < Poly->PolyVerts.Num(); iVert++)
			{
				FVector VertLoc = Poly->GetVertLocation(iVert, WORLD_SPACE);
				Poly1WS.AddItem(VertLoc);
				Poly1Center += VertLoc;
			}
			Poly1Center /= Poly1WS.Num();

			for (INT iPoly = 0; iPoly < Polys.Num(); iPoly++)
			{
				FNavMeshPolyBase* CurPoly = Polys(iPoly);
				UNavigationMeshBase* OtherMesh = CurPoly->NavMesh;
				APylon* OtherPylon = (OtherMesh != NULL) ? OtherMesh->GetPylon() : NULL;

				if (OtherMesh == NavMeshPtr) continue;

				// don't touch overlapping polys when neither this pylon nor current intersecting one uses Recast
				if (NavMeshGenerator != NavMeshGenerator_Recast && (OtherPylon == NULL || OtherPylon->NavMeshGenerator != NavMeshGenerator_Recast))
					continue;

				// cache verts locations in world space
				static TArray<FVector> Poly2WS;
				Poly2WS.Reset();
				for (INT iVert = 0; iVert < CurPoly->PolyVerts.Num(); iVert++)
				{
					Poly2WS.AddItem(CurPoly->GetVertLocation(iVert, WORLD_SPACE));
				}

				if (IsAdjacentPoly(Poly1WS, Poly2WS)) continue;

				static TArray<FVector> IsectLocation;
				static TArray<INT> IsectEdgeIdx;
				IsectLocation.Reset();
				IsectEdgeIdx.Reset();

				// try to find intersection points between polys
				for (INT iEdge1 = 0; iEdge1 < Poly1WS.Num(); iEdge1++)
				{
					FVector Edge1V0 = Poly1WS(iEdge1);
					FVector Edge1V1 = Poly1WS((iEdge1 + 1) % Poly1WS.Num());

					for (INT iEdge2 = 0; iEdge2 < Poly2WS.Num(); iEdge2++)
					{
						FVector Edge2V0 = Poly2WS(iEdge2);
						FVector Edge2V1 = Poly2WS((iEdge2 + 1) % Poly2WS.Num());
					
						FVector PtOnEdge1, PtOnEdge2;
						SegmentDistToSegmentSafe(Edge1V0, Edge1V1, Edge2V0, Edge2V1, PtOnEdge1, PtOnEdge2);
					
						FLOAT DistPts = (PtOnEdge1 - PtOnEdge2).Size();
						if (DistPts < 1.0f && !ContainsSimilairPoint(IsectLocation, PtOnEdge1))
						{
							IsectLocation.AddItem(PtOnEdge1);
							IsectEdgeIdx.AddItem(iEdge1);
						}
					}
				}

				// skip, when polys don't overlap
				if (IsectLocation.Num() < 2) continue;

				INT ClipIdx1 = 0;
				INT ClipIdx2 = 1;

				// in case of multiple intersections, find 2 on the same edge of Poly (main loop)
				if (IsectLocation.Num() > 2)
				{
					UBOOL bFoundSameEdge = FALSE;
					for (INT iPoint1 = 0; iPoint1 < IsectEdgeIdx.Num() && !bFoundSameEdge; iPoint1++)
					{
						INT MatchEdgeIdx = IsectEdgeIdx(iPoint1);
						for (INT iPoint2 = iPoint1 + 1; iPoint2 < IsectEdgeIdx.Num(); iPoint2++)
						{
							if (IsectEdgeIdx(iPoint2) == MatchEdgeIdx)
							{
								ClipIdx1 = iPoint1;
								ClipIdx2 = iPoint2;
								bFoundSameEdge = TRUE;
								break;
							}
						}
					}
				}

				// build clipping plane (normal outside Poly)
				FVector ClipOrg = IsectLocation(ClipIdx1);
				FVector ClipEdgeDir = (IsectLocation(ClipIdx2) - ClipOrg).SafeNormal();
				FVector ClipPlaneNormal = -ClipEdgeDir ^ Poly->PolyNormal;
				FLOAT DirToClipDotPlaneNormal = ((ClipOrg - Poly1Center) | ClipPlaneNormal);
				if (DirToClipDotPlaneNormal < 0) ClipPlaneNormal *= -1.0f;

				FPlane ClipPlane(ClipOrg, ClipPlaneNormal);

				// split both polys with clipping plane
				static TArray<FVector> Poly1Part1Verts, Poly1Part2Verts;
				static TArray<FVector> Poly2Part1Verts, Poly2Part2Verts;
				static TArray<FNavMeshPolyBase*> NewPolys;
				Poly1Part1Verts.Reset(); Poly1Part2Verts.Reset();
				Poly2Part1Verts.Reset(); Poly2Part2Verts.Reset();

//				debugf(TEXT("Cutting overlapping poly:%d"), Poly->Item);
				Poly->NavMesh->SplitPolyAlongPlane(Poly, ClipPlane, Poly1Part1Verts, Poly1Part2Verts);
				CurPoly->NavMesh->SplitPolyAlongPlane(CurPoly, ClipPlane, Poly2Part1Verts, Poly2Part2Verts);

				// always save Poly1Part2 (inside Poly)
				UBOOL bSplitThisPoly = !IsNearlyEqualPoly(Poly, Poly1Part2Verts);
				if (bSplitThisPoly && Poly1Part2Verts.Num() >= 3)
				{
					Poly->NavMesh->AddPoly(Poly1Part2Verts, Poly->PolyHeight, WORLD_SPACE);
				}

				// always save Poly2Part1 (inside CurPoly)
				UBOOL bSplitOtherPoly = !IsNearlyEqualPoly(CurPoly, Poly2Part1Verts);
				if (bSplitOtherPoly && Poly2Part1Verts.Num() >= 3)
				{
					CurPoly->NavMesh->AddPoly(Poly2Part1Verts, CurPoly->PolyHeight, WORLD_SPACE);
				}

				// allow other parts only when their center is outside other poly
				if (bSplitThisPoly && Poly1Part1Verts != Poly1Part2Verts)
				{
					SplitDegeneratedPoly(Poly1Part1Verts, Poly, CurPoly);
				}

				if (bSplitOtherPoly && Poly2Part2Verts != Poly2Part1Verts)
				{
					SplitDegeneratedPoly(Poly2Part2Verts, CurPoly, Poly);
				}

				if (bSplitOtherPoly)
				{
					CurPoly->NavMesh->RemovePoly(CurPoly);
				}

				if (bSplitThisPoly)
				{
					//@note: have to defer, not safe to modify TDoubleLinkedList while iterating over the nodes
					PolysToRemove.AddItem(Poly);
					break;
				}
			}
		}
		for (INT i = 0; i < PolysToRemove.Num(); i++)
		{
			NavMeshPtr->RemovePoly(PolysToRemove(i));
		}
	}
#endif // WITH_RECAST
}

UBOOL APylon::NavMeshPass_SplitForImportedMeshes()
{
	GWarn->StatusUpdatef( CurrentPylonIndex, TotalNumOfPylons, TEXT("Splitting mesh for imported meshes") );
	SCOPE_QUICK_TIMERX(Splitting,TRUE)

	NavMeshPtr->SplitMeshForIntersectingImportedMeshes();

	return TRUE;
}

UBOOL APylon::NavMeshPass_SplitMeshAboutPathObjects()
{
	GWarn->StatusUpdatef( CurrentPylonIndex, TotalNumOfPylons, TEXT("Splitting mesh around path objects") );
	SCOPE_QUICK_TIMERX(SplittingForPathObjects,TRUE)

	NavMeshPtr->SplitMeshAboutPathObjects();

	return TRUE;
}


UBOOL APylon::NavMeshPass_FixupForSaving()
{
	if( !ExpansionDoSaveFixup )
	{
		return TRUE;
	}

	AWorldInfo *Info = GWorld->GetWorldInfo();

	GWarn->StatusUpdatef( CurrentPylonIndex, TotalNumOfPylons, TEXT("Fixing up mesh") );
	SCOPE_QUICK_TIMERX(FixupForSaving,TRUE)
	
	NavMeshPtr->FixupForSaving();

	return TRUE;
}

UBOOL APylon::NavMeshPass_BuildObstacleMesh()
{
	if( DO_OBSTACLEMESH_CREATION )
	{
		BuildObstacleMesh(NavMeshPtr,ObstacleMesh);
		ObstacleMesh->FixupForSaving();

		ObstacleMesh->BuildKDOP();

		if(ExpansionTestCollision)
		{
			TestCollision(NavMeshPtr,ObstacleMesh);
		}

	}

	return TRUE;
}


UForcedReachSpec* CreateSuperPathFromAToB(APylon* A, APylon* B)
{
	UForcedReachSpec* newSpec = NULL;
	// if specified a valid nav point
	if (A != NULL &&
		B != NULL &&
		A != B)
	{

		UClass* ReachSpecClass = UForcedReachSpec::StaticClass();
		// create the forced spec
		newSpec = ConstructObject<UForcedReachSpec>(ReachSpecClass,A->GetOuter(),NAME_None);

		newSpec->CollisionRadius = 0;
		newSpec->CollisionHeight = 0;
		newSpec->Start = A;
		newSpec->End = B;
		newSpec->Distance = appTrunc((A->Location - B->Location).Size());
		// and add the spec to the path list
		A->PathList.AddItem(newSpec);
	}

	return newSpec;
}
// Create super network between pylons
#if WITH_EDITOR
UBOOL APylon::NavMeshPass_BuildPylonToPylonReachSpecs()
{
	UNavigationMeshBase* NavMesh = GetNavMesh();
	if( NavMesh != NULL )
	{
		NavMesh->PopulateEdgePtrCache();
		// For each cross pylon edge
		for( INT CrossPyIdx = 0; CrossPyIdx < NavMesh->CrossPylonEdges.Num(); CrossPyIdx++ )
		{
			FNavMeshCrossPylonEdge* Edge = NavMesh->CrossPylonEdges(CrossPyIdx);
			FNavMeshPolyBase* P0 = Edge->GetPoly0();
			FNavMeshPolyBase* P1 = Edge->GetPoly1();

			// Grab the other pylon
			APylon* Pylon = NULL;
			
			if(P0 != NULL && P0->NavMesh != NULL && P1 != NULL && P1->NavMesh != NULL )
			{
				Pylon = (P0->NavMesh->GetPylon()!=this) ? P0->NavMesh->GetPylon() : P1->NavMesh->GetPylon();
			}
			
			if(Pylon == NULL || Pylon == this)
			{
				continue;
			}

			// If there is currently no reach spec between the pylons			
			UReachSpec* Spec = GetReachSpecTo( Pylon );
			if( Spec == NULL )
			{
				// Create another one
				Spec = CreateSuperPathFromAToB( this, Pylon );
				Spec->CollisionRadius = 0;
				Spec->CollisionHeight = 0;
			}

			// Update max allowed radius/height between the pylons
			Spec->CollisionRadius = ::Max<INT>( Spec->CollisionRadius, appTrunc(Edge->GetEdgeLength() * 0.5f) );
			Spec->CollisionHeight = ::Max<INT>( Spec->CollisionHeight, appTrunc(::Min<FLOAT>(P0->PolyHeight,P1->PolyHeight)));
		}
	}
	return TRUE;
}
#endif

///////////////////////////////////
///////// NAVIGATION MESH /////////
///////////////////////////////////

void FMeshVertex::DebugLog( INT Idx )
{
	debugf(TEXT("\tVertex %d"), Idx );
	debugf(TEXT("\t\tPosition %s"), *ToString());
	debugf(TEXT("\t\tCounts: %d/%d"), PolyIndices.Num(), ContainingPolys.Num());
	for( INT PolyIdx = 0; PolyIdx < PolyIndices.Num(); PolyIdx++ )
	{
		debugf(TEXT("\t\t%d PolyIdx: %d"), PolyIdx, PolyIndices(PolyIdx) );
	}
	for( INT PolyIdx = 0; PolyIdx < ContainingPolys.Num(); PolyIdx++ )
	{
		debugf(TEXT("\t\t\t%d ContainingIdx: %d"), PolyIdx, ContainingPolys(PolyIdx) );
	}
}



FArchive& operator<<( FArchive& Ar, FPolyReference& T )
{
	Ar << T.OwningPylon;
	Ar << T.PolyId;
	
	if(Ar.Ver() < VER_FPOLYREF_CHANGE)
	{
		// if this is an old version, assume subpoly ID is invalid, fixup remaining bits to fit the pattern
		// (top 2 bytes = top level poly ID, bottom 2 bytes = sub poly ID)		
		T.PolyId |= (65535<<16);
	}

	return Ar;
}


FNavMeshCrossPylonEdge::FNavMeshCrossPylonEdge( UNavigationMeshBase* OwningMesh,
											   APylon* Pylon0,
											   WORD Pylon0PolyIdx,
											   VERTID Pylon0IdxV1,
											   VERTID Pylon0IdxV2,
											   APylon* Pylon1,
											   WORD Pylon1PolyIdx,
											   VERTID Pylon1IdxV1,
											   VERTID Pylon1IdxV2 )
{
	NavMesh = OwningMesh;

	// only two verts here, but we add indecies for both meshes
	Vert0 = Pylon0IdxV1;
	Vert1 = Pylon0IdxV2;

	OtherPylonVert0 = Pylon1IdxV1;
	OtherPylonVert1 = Pylon1IdxV2;

	UNavigationMeshBase* TempMesh = OwningMesh;
	FLOAT EdgeLength = (TempMesh->GetVertLocation(Pylon0IdxV1,LOCAL_SPACE)-TempMesh->GetVertLocation(Pylon0IdxV2,LOCAL_SPACE)).Size();
	EffectiveEdgeLength = EdgeLength;
//	checkSlowish(EdgeLength > KINDA_SMALL_NUMBER);
	UpdateEdgeCenter( TempMesh );

	// set up poly refs
	Poly0Ref = FPolyReference(Pylon0,Pylon0PolyIdx);
	Poly1Ref = FPolyReference(Pylon1,Pylon1PolyIdx);
	ObstaclePolyID = MAXWORD;

	bPendingDelete=FALSE;
	bIsCrossPylon=TRUE;
}

FNavMeshCrossPylonEdge::FNavMeshCrossPylonEdge( UNavigationMeshBase* OwningMesh,
					   FNavMeshPolyBase* Poly0,
					   VERTID Pylon0IdxV1,
					   VERTID Pylon0IdxV2,
					   FNavMeshPolyBase* Poly1,
					   VERTID Pylon1IdxV1,
					   VERTID Pylon1IdxV2 )
{
	NavMesh = OwningMesh;

	// only two verts here, but we add indecies for both meshes
	Vert0 = Pylon0IdxV1;
	Vert1 = Pylon0IdxV2;

	OtherPylonVert0 = Pylon1IdxV1;
	OtherPylonVert1 = Pylon1IdxV2;
	
	UNavigationMeshBase* TempMesh = Poly0->NavMesh;

	FLOAT EdgeLength = (TempMesh->GetVertLocation(Pylon0IdxV1,LOCAL_SPACE)-TempMesh->GetVertLocation(Pylon0IdxV2,LOCAL_SPACE)).Size();
	EffectiveEdgeLength = EdgeLength;
	//	checkSlowish(EdgeLength > KINDA_SMALL_NUMBER);

	// set up poly refs
	Poly0Ref = FPolyReference(Poly0);
	Poly1Ref = FPolyReference(Poly1);
	ObstaclePolyID = MAXWORD;

	bIsCrossPylon=TRUE;
	bPendingDelete=FALSE;

	UpdateEdgeCenter( TempMesh );

}

FNavMeshDropDownEdge::FNavMeshDropDownEdge( UNavigationMeshBase* Mesh, VERTID IdxV1, VERTID IdxV2 )
{
	Vert0 = IdxV1;
	Vert1 = IdxV2;

	//	checkSlowish(EdgeLength > KINDA_SMALL_NUMBER); //ss: breaks coverslip edges which are from a single point
	UpdateEdgeCenter( Mesh );

	ObstaclePolyID = MAXWORD;
	EdgeType = GetEdgeType();
	bIsCrossPylon=TRUE;
}


FArchive& FNavMeshDropDownEdge::Serialize( FArchive& Ar )
{
	FNavMeshCrossPylonEdge::Serialize( Ar );

	if(NavMesh->NavMeshVersionNum >= VER_DROPHEIGHT_SERIALIZATION)
	{
		Ar << DropHeight;
	}
	else if(Ar.IsLoading())
	{
		DropHeight = 0.f; // disable the edge since we don't have a valid drop height!
	}

	EdgeType = GetEdgeType();

	return Ar;
}


FNavMeshPolyBase* FNavMeshCrossPylonEdge::GetOtherPoly( FNavMeshPolyBase* Poly )
{
	if(!Poly0Ref || !Poly1Ref)
	{
		return NULL;
	}

	return (*Poly0Ref == Poly) ? *Poly1Ref : *Poly0Ref;
}

FVector FNavMeshCrossPylonEdge::GetVertLocation(INT LocalIdx, UBOOL bWorldSpace)
{
	switch( LocalIdx )
	{
	case 0:
		return NavMesh->GetVertLocation(Vert0,bWorldSpace);
	case 1:
		return NavMesh->GetVertLocation(Vert1,bWorldSpace);
	case 2:
		return NavMesh->GetVertLocation(OtherPylonVert0,bWorldSpace);
	case 3:
		return NavMesh->GetVertLocation(OtherPylonVert1,bWorldSpace);
	default:
		check(FALSE && "Invalid vert idx");
		return FVector(0.f);
	}
	
}

FNavMeshPolyBase* FNavMeshCrossPylonEdge::CrossPylon_GetPoly0()
{
	if( bPendingDelete )
	{
		return NULL;
	}
	return *Poly0Ref;
}

FNavMeshPolyBase* FNavMeshCrossPylonEdge::CrossPylon_GetPoly1()
{
	if( bPendingDelete )
	{
		return NULL;
	}
	return *Poly1Ref;
}

void FNavMeshCrossPylonEdge::SetPoly0(FNavMeshPolyBase* InPoly)
{
	Poly0Ref = InPoly;
}

void FNavMeshCrossPylonEdge::SetPoly1(FNavMeshPolyBase* InPoly)
{
	Poly1Ref = InPoly;
}

UBOOL FNavMeshCrossPylonEdge::IsValid(UBOOL bAllowTopLevelEdgesWhenSubMeshPresent)
{ 
	if ( bPendingDelete ||  !(Poly0Ref || Poly1Ref))
	{
		return FALSE;
	}

	return FNavMeshEdgeBase::IsValid(bAllowTopLevelEdgesWhenSubMeshPresent);
}

void UNavigationMeshBase::InitializeEdgeClasses()
{
	FNavMeshEdgeBase::Register();
	FNavMeshSpecialMoveEdge::Register();
	FNavMeshMantleEdge::Register();
	FNavMeshCoverSlipEdge::Register();
	FNavMeshCrossPylonEdge::Register();
	FNavMeshDropDownEdge::Register();
	FNavMeshPathObjectEdge::Register();
	FNavMeshBasicOneWayEdge::Register();
	FNavMeshOneWayBackRefEdge::Register();
}

IMPLEMENT_EDGE_CLASS(FNavMeshEdgeBase)
IMPLEMENT_EDGE_CLASS(FNavMeshBasicOneWayEdge)
IMPLEMENT_EDGE_CLASS(FNavMeshCrossPylonEdge)
IMPLEMENT_EDGE_CLASS(FNavMeshDropDownEdge)
IMPLEMENT_EDGE_CLASS(FNavMeshOneWayBackRefEdge)

/**
 * this function is called after a poly linked to this edge is replaced with a submesh for a pathobstacle
 * allows special edges to have a chance to add extra data after the mesh is split
 * @param Poly - the poly that was just disabled and replaced with a submesh
 * @param NewSubMesh - the submesh that now represents the poly
 * @param bFromBackRef - are we adding edges from a backreference? (e.g. B just got rebuild, and we're doing edge adds for an edge that points from A->B)
 */
void FNavMeshOneWayBackRefEdge::PostSubMeshUpdateForOwningPoly(FNavMeshPolyBase* Poly, UNavigationMeshBase* New_SubMesh, UBOOL bFromBackRef)
{
	// don't follow back refs back to the guy being rebuilt in the first place
	if ( bFromBackRef )
	{
		return;
	}

	FNavMeshPolyBase* SourcePoly = GetPoly1();
	
	// if the source poly is disabled/unloaded just bail!
	if ( SourcePoly == NULL )
	{
		return;
	}
	// call on all incoming one-way edges 
	FNavMeshEdgeBase* Current_Edge = NULL;
	FNavMeshPolyBase* Other_Poly = NULL;
	for(INT SourceEdgeIdx=0;SourceEdgeIdx<SourcePoly->GetNumEdges();++SourceEdgeIdx)
	{
		Current_Edge = SourcePoly->GetEdgeFromIdx(SourceEdgeIdx,NULL,TRUE);
		
		if ( Current_Edge == NULL || !Current_Edge->IsOneWayEdge() || !Current_Edge->IsValid(TRUE))
		{
			continue;
		}
		Other_Poly = Current_Edge->GetOtherPoly(SourcePoly);

		if ( Other_Poly == Poly )
		{
			// add linkages back from current edge's navmesh
			Current_Edge->PostSubMeshUpdateForOwningPoly(Poly,Current_Edge->NavMesh, TRUE);
		}
	}	
}


FNavMeshEdgeBase::FNavMeshEdgeBase( UNavigationMeshBase* Mesh, VERTID IdxV1, VERTID IdxV2 ) :
	FNavMeshObject(Mesh),
	bAlreadyVisited(FALSE),
	bNotLongestEdgeInGroup(FALSE),
	bPendingDelete(FALSE),
	bIsCrossPylon(FALSE),
	Vert0(IdxV1),
	Vert1(IdxV2),	
	EdgeType(0),
	ExtraEdgeCost(0),
	EdgeGroupID(MAXBYTE),
	VisitedPathWeight(-1),
	EstimatedOverallPathWeight(-1),	
	NextOpenOrdered(NULL),
	PrevOpenOrdered(NULL),
	PreviousPathEdge(NULL),
	PreviousPosition(0.f),
	SavedSessionID(MAXDWORD),
	DestinationPolyID(0)
{
	UpdateEdgeCenter( Mesh );

	// default to -1 indicating it needs to be computed
	EffectiveEdgeLength=-1.f;
}

IMPLEMENT_EDGE_CLASS(FNavMeshSpecialMoveEdge)
FNavMeshSpecialMoveEdge::FNavMeshSpecialMoveEdge( UNavigationMeshBase* OwningMesh,
												 APylon* Pylon0,
												 WORD Pylon0PolyIdx,
												 VERTID Pylon0IdxV1,
												 VERTID Pylon0IdxV2,
												 APylon* Pylon1,
												 WORD Pylon1PolyIdx,
												 VERTID Pylon1IdxV1,
												 VERTID Pylon1IdxV2 ) :
FNavMeshCrossPylonEdge( OwningMesh,Pylon0, Pylon0PolyIdx, Pylon0IdxV1, Pylon0IdxV2,  Pylon1, Pylon1PolyIdx, Pylon1IdxV1, Pylon1IdxV2 )
, MoveDest(EC_EventParm)
, MoveDir(0)
{
}

FNavMeshSpecialMoveEdge::FNavMeshSpecialMoveEdge( UNavigationMeshBase* OwningMesh,
						struct FNavMeshPolyBase* Poly0,
						VERTID Pylon0IdxV1,
						VERTID Pylon0IdxV2,
						struct FNavMeshPolyBase* Poly1,
						VERTID Pylon1IdxV1,
						VERTID Pylon1IdxV2 ) :
FNavMeshCrossPylonEdge(OwningMesh,Poly0,Pylon0IdxV1,Pylon0IdxV2,Poly1,Pylon1IdxV1,Pylon1IdxV2)
, MoveDest(EC_EventParm)
, MoveDir(0)
{
}

/**
 * this function is called after a poly linked to this edge is replaced with a submesh for a pathobstacle
 * allows special edges to have a chance to add extra data after the mesh is split
 * @param Poly - the poly that was just disabled and replaced with a submesh
 * @param NewSubMesh - the submesh that now represents the poly
 * @param bFromBackRef - are we adding edges from a backreference? (e.g. B just got rebuild, and we're doing edge adds for an edge that points from A->B)
 */
void FNavMeshSpecialMoveEdge::PostSubMeshUpdateForOwningPoly(FNavMeshPolyBase* Poly, UNavigationMeshBase* New_SubMesh, UBOOL bFromBackRef/*=FALSE*/)
{
	// build list of poly spans in new submesh, then iterate over each span and find polys on the other end to link to 
	const FVector EdgeStart = GetVertLocation(0,WORLD_SPACE);
	const FVector EdgeEnd = GetVertLocation(1,WORLD_SPACE);

	// generate direction and offset to use for destination span checks
	const FVector Ctr = (EdgeStart + EdgeEnd)*0.5f;
	const FVector Delta = *MoveDest - Ctr;
	const FVector Dir = Delta.SafeNormal();

	// save off orig starting poly
	FNavMeshPolyBase* Old_Poly0 = GetPoly0();
	
	TArray<FPolySegmentSpan> Source_Spans,Dest_Spans;

	if( GetEdgeLength() < 10.f )
	{
		FNavMeshPolyBase* SourcePoly = New_SubMesh->GetPolyFromPoint(EdgeStart,0.707F,WORLD_SPACE);
		FNavMeshPolyBase* DestPoly = NULL;
		
		APylon* Py = NULL;
		const FVector DestPt = Ctr+Delta;
		if( SourcePoly != NULL && UNavigationHandle::GetPylonAndPolyFromPos(DestPt,0.707f,Py,DestPoly) )
		{
			if (SourcePoly != DestPoly)
			{
				FVector SrcPolyCtr = SourcePoly->GetPolyCenter();
				FVector SrcPolyCtr_Orig=SrcPolyCtr;

				Old_Poly0->AdjustPositionToDesiredHeightAbovePoly(SrcPolyCtr,0.f);
				FLOAT Dist = (SrcPolyCtr_Orig-SrcPolyCtr).Size();

				if( Dist < NAVMESHGEN_ENTITY_HALF_HEIGHT )
				{
					FPolySegmentSpan SourceSpan = FPolySegmentSpan(SourcePoly,EdgeStart,EdgeStart);
					FPolySegmentSpan DestSpan = FPolySegmentSpan(DestPoly,DestPt,DestPt);
					AddDynamicEdgeForSpan(New_SubMesh,SourceSpan,DestSpan);
				}
			}
		}	
	}
	else
	{
		New_SubMesh->GetPolySegmentSpanList(EdgeStart,EdgeEnd,Source_Spans,WORLD_SPACE);
		for(INT SourceSpanIdx=0;SourceSpanIdx<Source_Spans.Num();++SourceSpanIdx)
		{
			FPolySegmentSpan& Source_Span = Source_Spans(SourceSpanIdx);

			// for this span, find all the spans on the other side of the edge 
			FVector DestEndPoint1 = Source_Span.P1 + Delta;
			FVector DestEndPoint2 = Source_Span.P2 + Delta;

			Dest_Spans.Empty();
			UNavigationHandle::GetPolySegmentSpanList(DestEndPoint1,DestEndPoint2,Dest_Spans);
			// for each destination span, add a new edge from submesh to dest poly
			for(INT Dest_SpanIdx=0;Dest_SpanIdx<Dest_Spans.Num();++Dest_SpanIdx)
			{
				FPolySegmentSpan& Dest_Span = Dest_Spans(Dest_SpanIdx);

				if(Dest_Span.Poly == Source_Span.Poly)
				{
					continue;
				}


				FVector SrcPolyCtr = Source_Span.Poly->GetPolyCenter();

				FVector SrcPolyCtr_Orig=SrcPolyCtr;

				Old_Poly0->AdjustPositionToDesiredHeightAbovePoly(SrcPolyCtr,0.f);
				FLOAT Dist = (SrcPolyCtr_Orig-SrcPolyCtr).Size();

				if( Dist < NAVMESHGEN_ENTITY_HALF_HEIGHT )
				{
					AddDynamicEdgeForSpan(New_SubMesh,Source_Span,Dest_Span);
				}
			}
		}
	}
	
}

/**
 * called when a special move edge is being re-fixed up after an obstacle has changed the mesh
 * allows each subclass to add it own proper type
 * is responsible for adding a new edge represented by the two spans
 * @param Source_Span - Span for source 
 * @param Dest_Span - Span for dest
 */
void FNavMeshSpecialMoveEdge::AddDynamicEdgeForSpan(UNavigationMeshBase* NavMesh, struct FPolySegmentSpan& Source_Span, struct FPolySegmentSpan& Dest_Span)
{
	static TArray<FNavMeshPolyBase*> ConnectedPolys;
	ConnectedPolys.Reset(2);
	ConnectedPolys.AddItem(Source_Span.Poly);
	ConnectedPolys.AddItem(Dest_Span.Poly);

	FNavMeshSpecialMoveEdge* NewEdge = AddTypedEdgeForObstacleReStitch(NavMesh, Source_Span.P1,Source_Span.P2,ConnectedPolys);
	if(NewEdge != NULL)
	{
		FVector Pos = (Dest_Span.P1 + Dest_Span.P2)/2.f;
		NewEdge->MoveDest.Set( (RelActor.Actor!=NULL)?RelActor->Base:NULL, Pos);
		NewEdge->MoveDir  = MoveDir;
		NewEdge->RelActor = RelActor;
		NewEdge->RelItem  = RelItem;
		NewEdge->EffectiveEdgeLength = EffectiveEdgeLength;
		NewEdge->EdgeGroupID = EdgeGroupID;
	}
}

IMPLEMENT_EDGE_CLASS(FNavMeshMantleEdge)
FNavMeshMantleEdge::FNavMeshMantleEdge( UNavigationMeshBase* OwningMesh,
									   APylon* Pylon0,
									   WORD Pylon0PolyIdx,
									   VERTID Pylon0IdxV1,
									   VERTID Pylon0IdxV2,
									   APylon* Pylon1,
									   WORD Pylon1PolyIdx,
									   VERTID Pylon1IdxV1,
									   VERTID Pylon1IdxV2 ) :
FNavMeshSpecialMoveEdge( OwningMesh, Pylon0, Pylon0PolyIdx, Pylon0IdxV1, Pylon0IdxV2, Pylon1, Pylon1PolyIdx, Pylon1IdxV1, Pylon1IdxV2 )
{
	EdgeType = NAVEDGE_Mantle;
}

FNavMeshMantleEdge::FNavMeshMantleEdge( UNavigationMeshBase* OwningMesh,
	struct FNavMeshPolyBase* Poly0,
	VERTID Pylon0IdxV1,
	VERTID Pylon0IdxV2,
	struct FNavMeshPolyBase* Poly1,
	VERTID Pylon1IdxV1,
	VERTID Pylon1IdxV2 ) :
FNavMeshSpecialMoveEdge(OwningMesh,Poly0,Pylon0IdxV1,Pylon0IdxV2,Poly1,Pylon1IdxV1,Pylon1IdxV2)
{
	EdgeType = NAVEDGE_Mantle;
}

IMPLEMENT_EDGE_CLASS(FNavMeshCoverSlipEdge)
FNavMeshCoverSlipEdge::FNavMeshCoverSlipEdge( UNavigationMeshBase* OwningMesh,
											 APylon* Pylon0,
											 WORD Pylon0PolyIdx,
											 VERTID Pylon0IdxV1,
											 VERTID Pylon0IdxV2,
											 APylon* Pylon1,
											 WORD Pylon1PolyIdx,
											 VERTID Pylon1IdxV1,
											 VERTID Pylon1IdxV2 ) :
FNavMeshSpecialMoveEdge( OwningMesh, Pylon0, Pylon0PolyIdx, Pylon0IdxV1, Pylon0IdxV2, Pylon1, Pylon1PolyIdx, Pylon1IdxV1, Pylon1IdxV2 )
{
	EdgeType = NAVEDGE_Coverslip;
}

FNavMeshCoverSlipEdge::FNavMeshCoverSlipEdge( UNavigationMeshBase* OwningMesh,
											  struct FNavMeshPolyBase* Poly0,
											  VERTID Pylon0IdxV1,
											  VERTID Pylon0IdxV2,
											  struct FNavMeshPolyBase* Poly1,
											  VERTID Pylon1IdxV1,
											  VERTID Pylon1IdxV2 ) :
FNavMeshSpecialMoveEdge(OwningMesh,Poly0,Pylon0IdxV1,Pylon0IdxV2,Poly1,Pylon1IdxV1,Pylon1IdxV2)
{
	EdgeType = NAVEDGE_Coverslip;
}

FArchive& FNavMeshEdgeBase::Serialize( FArchive& Ar )
{
	
	SerializeEdgeVerts(Ar);

	Ar << Poly0;
	Ar << Poly1;

	// if this is old data account for edgelength float still being in the stream
	if( NavMesh != NULL && NavMesh->NavMeshVersionNum < VER_REMOVED_EDGELENGTH )
	{
		FLOAT DummyFloat;
		Ar << DummyFloat;
	}

	if(NavMesh != NULL && NavMesh->NavMeshVersionNum >= VER_EFFECTIVE_EDGE_LEN)
	{
		Ar << EffectiveEdgeLength;
	}
	else
	{
		if(!IsCrossPylon())
		{
			EffectiveEdgeLength = (GetVertLocation(0,LOCAL_SPACE) - GetVertLocation(1,LOCAL_SPACE)).Size();
		}
		else
		{
			EffectiveEdgeLength=-1.0f;
		}
	}
	Ar << EdgeCenter;
	Ar << EdgeType;

	if(NavMesh != NULL && NavMesh->NavMeshVersionNum >= VER_EDGE_GROUPS)
	{
		Ar << EdgeGroupID;
	}

	if(NavMesh != NULL && NavMesh->NavMeshVersionNum >= VER_EDGEPERP)
	{
		Ar << EdgePerp;
	}
	else
	{
		EdgePerp = FVector(0.f);
	}



	return Ar;
}

void FNavMeshEdgeBase::SerializeEdgeVerts( FArchive& Ar )
{
	if(NavMesh != NULL && NavMesh->NavMeshVersionNum >= VER_REMOVED_EDGEVERTS_ARRAY)
	{
		Ar << Vert0;
		Ar << Vert1;
	}
	else
	{
		TArray<VERTID> EdgeVerts;
		if( Ar.IsSaving() )
		{
			EdgeVerts.AddItem(Vert0);
			EdgeVerts.AddItem(Vert1);
		}
		Ar << EdgeVerts;
		if ( EdgeVerts.Num() > 0 )
		{
			Vert0 = EdgeVerts(0);
			Vert1 = EdgeVerts(1);
		}
	}
}

FArchive& FNavMeshSpecialMoveEdge::Serialize( FArchive& Ar )
{
	if(NavMesh->NavMeshVersionNum < VER_SPECIALMOVEEDGES_CROSS_PYLON)
	{
		// if this is old data, construct a new cross-pylon edge out of the old data
		FNavMeshEdgeBase::Serialize( Ar );
		SetPoly0(FNavMeshEdgeBase::GetPoly0());
		SetPoly1(FNavMeshEdgeBase::GetPoly1());
	}
	else
	{
		FNavMeshCrossPylonEdge::Serialize( Ar );
	}

	if(NavMesh->NavMeshVersionNum < VER_VERNUM_RELACTORACTORREF)
	{
		AActor* TempRelActor = NULL;
		Ar << TempRelActor;
		RelActor = TempRelActor;
	}
	else
	{
		Ar << RelActor;
	}
	
	Ar << RelItem;
	Ar << MoveDest;
	Ar << MoveDir;

	return Ar;
}

FVector FNavMeshSpecialMoveEdge::GetEdgeDestination( const FNavMeshPathParams& PathParams,
													FLOAT EntityRadius,
												   const FVector& InfluencePosition,
												   const FVector& EntityPosition,
												   UNavigationHandle* Handle,
												   UBOOL bFirstPass )
{
	return GetEdgeCenter()+PathParams.Interface->GetEdgeZAdjust(this);
}


FArchive& FNavMeshCrossPylonEdge::Serialize( FArchive& Ar )
{
	FNavMeshEdgeBase::Serialize( Ar );

	if(NavMesh->NavMeshVersionNum >= VER_CROSSPYLONEDGESERIALIZATION)
	{
		Ar << Poly0Ref;
		Ar << Poly1Ref;
	}

	if(NavMesh != NULL && NavMesh->NavMeshVersionNum >= VER_SERIALIZE_OBSTACLEPOLYID)
	{
		Ar << ObstaclePolyID;
	}

	return Ar;
}

void FNavMeshCrossPylonEdge::SerializeEdgeVerts( FArchive& Ar )
{
	if(NavMesh != NULL && NavMesh->NavMeshVersionNum >= VER_REMOVED_EDGEVERTS_ARRAY)
	{
		Ar << Vert0;
		Ar << Vert1;
		Ar << OtherPylonVert0;
		Ar << OtherPylonVert1;
	}
	else
	{
		TArray<VERTID> EdgeVerts;
		if( Ar.IsSaving() )
		{
			EdgeVerts.AddItem(Vert0);
			EdgeVerts.AddItem(Vert1);
			EdgeVerts.AddItem(OtherPylonVert0);
			EdgeVerts.AddItem(OtherPylonVert1);
		}
		Ar << EdgeVerts;

		if ( EdgeVerts.Num() > 0 )
		{
			Vert0 = EdgeVerts(0);
			Vert1 = EdgeVerts(1);
			OtherPylonVert0 = EdgeVerts(2);
			OtherPylonVert1 = EdgeVerts(3);
		}
	}
}
/**
 *	will return the center of the edge
 * @param bWorldSpace - true if you want the center in world space
 * @return the center of the edge
 */
FVector FNavMeshEdgeBase::GetEdgeCenter(UBOOL bWorldSpace/* =WORLD_SPACE */)
{
	return (NavMesh != NULL && bWorldSpace) ? FVector(NavMesh->L2WTransformFVector(EdgeCenter)) : EdgeCenter;
}
/** 
 * Sets EdgeCenter variable based on the Edge Verts
 */
void FNavMeshEdgeBase::UpdateEdgeCenter( UNavigationMeshBase* InNavMesh )
{
	if( InNavMesh )
	{
		const FVector Vert0Loc = InNavMesh->Verts(Vert0);
		const FVector Vert1Loc = InNavMesh->Verts(Vert1);
		EdgeCenter = (Vert0Loc+Vert1Loc) * 0.5f;
	}
}

/**
 * Sets EdgePerp cache value
 */
void FNavMeshEdgeBase::UpdateEdgePerpDir()
{
	if( NavMesh )
	{
		const FVector Vert0Loc = NavMesh->Verts(Vert0);
		const FVector Vert1Loc = NavMesh->Verts(Vert1);
		EdgeCenter = (Vert0Loc+Vert1Loc) * 0.5f;

		FVector ThisEdgeDir = Vert0Loc - Vert1Loc;
		EdgePerp = (ThisEdgeDir ^ GetEdgeNormal(LOCAL_SPACE)).SafeNormal();
	}
}

/**
 *	will get the vertex location at the specified local index 
 * @param LocalIdx - local index of vert you want a location for
 * @param bWorldSpace - whether you want the loc in world space or not
 * @return the vertex location
 */
FVector FNavMeshEdgeBase::GetVertLocation(INT LocalIdx, UBOOL bWorldSpace)
{
	checkSlowish(LocalIdx < 2 && LocalIdx >= 0);
	return NavMesh->GetVertLocation((LocalIdx==0) ? Vert0 : Vert1,bWorldSpace);
}

/**
 *	returns the first poly associated with this edge
 * @return the first poly associated with this edge
 */
FNavMeshPolyBase* FNavMeshEdgeBase::GetPoly0()
{
	if ( !bPendingDelete && !IsCrossPylon() )
	{
		FNavMeshPolyBase* Poly = &NavMesh->Polys(Poly0);
		// 	if(Poly->NavMesh == NULL)
		// 	{
		// 		Poly->Item = Poly0;
		// 		Poly->NavMesh = NavMesh;
		// 	}
		checkSlowish(Poly->NavMesh != NULL);
		return Poly;
	}

	return IsCrossPylon() ? CrossPylon_GetPoly0() : NULL;
}

/**
 *	returns the second poly associated with this edge
 * @return the second poly associated with this edge
 */
FNavMeshPolyBase* FNavMeshEdgeBase::GetPoly1()
{
	if ( !bPendingDelete && !IsCrossPylon() )
	{
		FNavMeshPolyBase* Poly = &NavMesh->Polys(Poly1);
		if(Poly->NavMesh == NULL) 
		{
			Poly->NavMesh = NavMesh;
			Poly->Item = Poly1;
		}
		return Poly;
	}

	return IsCrossPylon() ? CrossPylon_GetPoly1() : NULL;
}

/**
 *	sets the first poly associated with this edge
 * @param Poly0 the first poly to be associated with this edge
 */
void FNavMeshEdgeBase::SetPoly0(FNavMeshPolyBase* InPoly0)
{
	Poly0 = InPoly0->Item;
}

/**
 *	sets the second poly associated with this edge
 * @param Poly0 the second poly to be associated with this edge
 */
void FNavMeshEdgeBase::SetPoly1(FNavMeshPolyBase* InPoly1)
{
	Poly1 = InPoly1->Item;
}

// useful for 'flattening' vectors according to the up vector passed (e.g. making vectors 2D about arbitrary up normals)
void aFlattenVectAlongPassedAxis(FVector& Vect, const FVector& Up)
{
	Vect -= (Vect | Up) * Up;
}

FVector aGetFlattenVectAlongPassedAxis(const FVector& Vect, const FVector& Up)
{
	return Vect - ((Vect | Up) * Up);
}

FLOAT aGetFlattenedDistanceBetweenVects(const FVector& A, const FVector& B, const FVector& Up)
{
	FVector Delta = (A-B);
	aFlattenVectAlongPassedAxis(Delta,Up);
	return Delta.Size();
}

FLOAT aGetFlattenedDistanceBetweenVectsSq(const FVector& A, const FVector& B, const FVector& Up)
{
	FVector Delta = (A-B);
	aFlattenVectAlongPassedAxis(Delta,Up);
	return Delta.SizeSquared();
}

/** 
 * returns the point that the passed searcher should run to in order to pass over this edge
 * (will return a safe point along the edge to run to that does not collide)
 * @param EntityRadius - the radius of the entity we need to build an edge position for
 * @param InfluencePosition - the position we should try and get close to with our edge position
 * @param EntityPosition    - the position the entity will be in when it starts moving toward this edge
 * @param Handle			- the handle that's requesting the edge position
 * @param bFirstPass		- when TRUE do not do kisspoint (corner) avoidance, only take closest point along edge
 */
#define DEBUG_GETEDGEDEST 0
#define DEBUG_PATHLANE 0
FVector FNavMeshEdgeBase::GetEdgeDestination( const FNavMeshPathParams& PathParams,
											 FLOAT InEntityRadius,
											 const FVector& InfluencePosition,
											 const FVector& InEntityPosition,
											 UNavigationHandle* Handle,
											 UBOOL bFirstPass )
{
	FLOAT EntityRadius = InEntityRadius;
	// Find closest vert and move away from it along the edge by radius
	FVector ClosestEdgePoint=FVector(0.f);
	FVector V0 = GetVertLocation(0);
	FVector V0_orig = V0;

	FVector V1 = GetVertLocation(1);
	FVector V1_orig = V1;


	FVector EntityPosition = InEntityPosition;

	FNavMeshPolyBase* PolyEntIsIn = NULL;
	FNavMeshPolyBase* Poly0 = GetPoly0();
	FNavMeshPolyBase* Poly1 = GetPoly1();
	if( Poly0 != NULL && Poly0->ContainsPoint(EntityPosition,WORLD_SPACE))
	{
		PolyEntIsIn = Poly0;
	}
	else if(Poly1 != NULL && Poly1->ContainsPoint(EntityPosition,WORLD_SPACE))
	{
		PolyEntIsIn = Poly1;
	}


	if(PolyEntIsIn != NULL)
	{
		EntityPosition = FPointPlaneProject(EntityPosition,PolyEntIsIn->GetPolyCenter(WORLD_SPACE),PolyEntIsIn->GetPolyNormal(WORLD_SPACE));
		// Flatten the entry position to line up with the edge height in world space to avoid flattening errors along edge normal (which may not be vertical)
		EntityPosition.Z = GetEdgeCenter(WORLD_SPACE).Z;
	}
	else
	// if not in either poly find closest point on either plane
	{
		EntityPosition.Z -= PathParams.SearchExtent.Z;
	}

	
	// 1. find closest point on edge to influence point
	PointDistToSegment( InfluencePosition, V0, V1, ClosestEdgePoint );
	// START PATH LANES
	if( PathParams.SearchLaneMultiplier > KINDA_SMALL_NUMBER )
	{
		const FLOAT PathLaneRadius = Min<FLOAT>((EntityRadius * 2.f) * PathParams.SearchLaneMultiplier, GetEdgeLength());
		const FLOAT DistSqToEdge = (ClosestEdgePoint - EntityPosition).SizeSquared();

// 		GWorld->GetWorldInfo()->DrawDebugBox( ClosestEdgePoint, FVector(5), 255, 0, 0, TRUE );
// 		GWorld->GetWorldInfo()->DrawDebugBox( InfluencePosition, FVector(4), 0, 255, 0, TRUE );
// 		GWorld->GetWorldInfo()->DrawDebugBox( EntityPosition, FVector(3), 0, 0, 255, TRUE );

		if( DistSqToEdge > EntityRadius*EntityRadius )
		{
			const FLOAT DistSqToVert0 = (ClosestEdgePoint - V0).SizeSquared();
			const FLOAT DistSqToVert1 = (ClosestEdgePoint - V1).SizeSquared();

			const FVector ClosestVert = DistSqToVert0 < DistSqToVert1 ? V0 : V1;
			const FVector EdgeDir = (V1 - V0).SafeNormal() * (DistSqToVert0 < DistSqToVert1 ? 1 : -1);
				
			const FLOAT DistToClosestVert = (ClosestEdgePoint - ClosestVert).SizeSquared();
			if( DistToClosestVert < (PathLaneRadius * PathLaneRadius) )
			{
#if DEBUG_PATHLANE
				FVector OrigClosestEdgePoint = ClosestEdgePoint;
#endif

				ClosestEdgePoint = ClosestVert + EdgeDir * PathLaneRadius;

#if DEBUG_PATHLANE			
				GWorld->GetWorldInfo()->DrawDebugBox( OrigClosestEdgePoint, FVector(5), 255, 0, 0, TRUE );
				GWorld->GetWorldInfo()->DrawDebugBox( ClosestEdgePoint, FVector(4), 0, 255, 0, TRUE );
				GWorld->GetWorldInfo()->DrawDebugBox( ClosestVert, FVector(3), 0, 0, 255, TRUE );

				GWorld->GetWorldInfo()->DrawDebugLine( OrigClosestEdgePoint, ClosestEdgePoint, 255, 0, 0, TRUE );
				GWorld->GetWorldInfo()->DrawDebugLine( ClosestVert, ClosestEdgePoint, 0, 0, 255, TRUE );
#endif
			}
		}
	}
	// END PATH LANES

	if( bFirstPass )
	{
		return ClosestEdgePoint;
	}

	// failsafe
	if( PolyEntIsIn == NULL && (InfluencePosition-EntityPosition).SizeSquared() > EntityRadius*EntityRadius )
	{
		return GetEdgeCenter(WORLD_SPACE);
	}


	const FVector EdgeDir = (V0 - V1).SafeNormal();

	const FLOAT DistFromEdgeRey = PointDistToLine(EntityPosition,EdgeDir,V0);
	const FLOAT DistToEdge = PointDistToEdge(EntityPosition);
	
	// then first move to a position further away from the edge line
	FVector EdgePerp = GetEdgePerpDir(WORLD_SPACE);

	FVector PerpTestLoc = EntityPosition;
	if( PolyEntIsIn != NULL )
	{
		PerpTestLoc=PolyEntIsIn->GetPolyCenter(WORLD_SPACE);
	}

	if( (EdgePerp | (PerpTestLoc-V0).SafeNormal()) < 0.f )
	{
		EdgePerp *= -1.0f;
	}

	FVector DestinationPosition(0.f);
		
		
	UBOOL bDoPointReachableLoop=FALSE;

	// Adjust the closest point above the occupied poly to avoid PointReachable failure
	FVector AdjustedClosestEdgePoint = ClosestEdgePoint;
	if( PolyEntIsIn != NULL )
	{
		PolyEntIsIn->AdjustPositionToDesiredHeightAbovePoly( AdjustedClosestEdgePoint, PathParams.SearchExtent.Z, TRUE );
	}

	//debug	
// 	if( !Handle->PointReachable(AdjustedClosestEdgePoint,InEntityPosition) )
// 	{
// 		GWorld->GetWorldInfo()->DrawDebugBox( AdjustedClosestEdgePoint, FVector(5), 255,0,0,TRUE);
// 		GWorld->GetWorldInfo()->DrawDebugBox( InEntityPosition, FVector(5), 0,255,0,TRUE);
// 	}
	
	if( Handle->PointReachable(AdjustedClosestEdgePoint,InEntityPosition) )
	{
		DestinationPosition = AdjustedClosestEdgePoint;
	}
	// if we're really close to the line of the edge, and also not actually close to the edge  (e.g. we just entered from another edge on the same line)
	else if( DistFromEdgeRey < PathParams.SearchExtent.X && DistToEdge > PathParams.SearchExtent.X )
	{
		DestinationPosition = EntityPosition+EdgePerp*PathParams.SearchExtent.X*1.414f;

		if( PolyEntIsIn == NULL || !PolyEntIsIn->ContainsPoint(DestinationPosition,WORLD_SPACE) )
		{
			bDoPointReachableLoop=TRUE;
		}
	}
	// if we're far away from the edge still, move to a point offset from the edge so we don't clip corners
	else
	{
		bDoPointReachableLoop=TRUE;
	}

	if( bDoPointReachableLoop )
	{
		const FVector OldIsectPt = ClosestEdgePoint;

		const FLOAT Step = EntityRadius*0.5f;
		FVector CurrentTestPos = ClosestEdgePoint;
		const FVector EdgeCtr = GetEdgeCenter(WORLD_SPACE);
		const FVector PtToCtr = (EdgeCtr - ClosestEdgePoint);
		const FLOAT MaxDist = PtToCtr.Size()*2.0f;
		const FVector StepDir = PtToCtr/MaxDist;
		FLOAT SteppedDist = 0.f;

		UBOOL bFoundOne = FALSE;
		while ( SteppedDist < MaxDist )
		{
			if( Handle->PointReachable(CurrentTestPos, InEntityPosition, FALSE) )
			{
				DestinationPosition = CurrentTestPos;
				bFoundOne=TRUE;
				break;
			}

			CurrentTestPos += StepDir * Step;
			SteppedDist+=Step;
		}

		if( !bFoundOne )
		{
			if( PolyEntIsIn == NULL 
				|| !PolyEntIsIn->GetBestLocationForCyl(InEntityPosition,InEntityRadius,PathParams.SearchExtent.Z,DestinationPosition,TRUE)
				|| (DestinationPosition-InEntityPosition).Size2D() < InEntityRadius*1.414f )
			{
				DestinationPosition = EdgeCtr;
			}
		}
	}

	return DestinationPosition+PathParams.Interface->GetEdgeZAdjust(this);
}

/**
 *	will return an appropriate normal for this edge (avg of two polys it connects)
 * @param bWorldSpace - should be TRUE if you want a normal in world space
 * @return - edge normal
 */
FVector FNavMeshEdgeBase::GetEdgeNormal(UBOOL bWorldSpace/* =WORLD_SPACE */)
{
	const FNavMeshPolyBase* RESTRICT Poly0 = GetPoly0();
	const FNavMeshPolyBase* RESTRICT Poly1 = GetPoly1();
	FVector Up = FVector(0.f,0.f,1.f);
	if(Poly0 == NULL || Poly1 == NULL)
	{
		return Up;
	}

	Up = Poly1->PolyNormal + Poly1->PolyNormal;
	Up *= 0.5f;

	if( !bWorldSpace )
	{
		return Up;		
	}
	
	Up = NavMesh->L2WTransformNormal(Up);
	return Up;
}

/**
 *	Will return the distance of the passed point to this edge
 *  @param InPoint - point to find distance from
 *  @param bWorldSpace- whether the incoming point is in WorldSpace or not
 *  @param out_ClosestPt - output param for closest point on the edge to the provided inPoint
 *  @return - distance from edge to inpoint
 */
FLOAT FNavMeshEdgeBase::PointDistToEdge(const FVector& InPoint, UBOOL bWorldSpace/*=WORLD_SPACE*/, FVector* out_ClosestPt)
{
	FVector LS_Pt = (bWorldSpace) ? FVector(NavMesh->W2LTransformFVector(InPoint)) : InPoint;
	FVector Closest(0.f);
	FLOAT Dist = PointDistToSegment(LS_Pt,GetVertLocation(0,LOCAL_SPACE),GetVertLocation(1,LOCAL_SPACE),Closest);

	if(out_ClosestPt != NULL)
	{
		*out_ClosestPt = (bWorldSpace) ? FVector(NavMesh->L2WTransformFVector(Closest)) : Closest;
	}
	return Dist;
}

/**
 *	Returns a vector perpendicular to this edge (edge norm cross edgedir)
 * @param bWorldSpace - whether this operation should be in world space or local space
 * @return - the perp vector
 */
FVector FNavMeshEdgeBase::GetEdgePerpDir(UBOOL bWorldSpace)
{
	if( (NavMesh != NULL && NavMesh->NavMeshVersionNum < VER_EDGEPERP) && EdgePerp.IsNearlyZero() )
	{
		UpdateEdgePerpDir();
	}

	if( !bWorldSpace )
	{
		return EdgePerp;
	}
	return NavMesh->L2WTransformNormal(EdgePerp);
}

/**
 * will do a line check against ONLY this edge's related obstacle mesh for its navmesh(es).  Does not do a full obstaclelinecheck (doesn't conform to terrain, or check walkable mesh)	
 * @param Result - output hit result describing the hit
 * @param End - endpoint of line check
 * @param Start - start point of linecheck
 * @param Extent - extent of box to sweep
 * @param TraceFlags - trace flags to pass to LineCheck()
 * @return TRUE if there was no hit
 */
UBOOL FNavMeshEdgeBase::LimitedObstacleLineCheck( FCheckResult& Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags )
{
	FNavMeshPolyBase* Poly0 = GetPoly0();
	FNavMeshPolyBase* Poly1 = GetPoly1();

	UNavigationMeshBase* Mesh1 = NULL;
	UNavigationMeshBase* Mesh2 = NULL;

	if (Poly0 != NULL)
	{
		Mesh1 = Poly0->NavMesh;
	}

	if (Poly1 != NULL)
	{
		Mesh2 = Poly1->NavMesh;
	}
	

	return ( (!Mesh1 || Mesh1->GetObstacleMesh()->LineCheck(Mesh1->GetTopLevelMesh(),Result,End,Start,Extent,TraceFlags)) && 
		(!Mesh2 || Mesh1 == Mesh2 || Mesh2->GetObstacleMesh()->LineCheck(Mesh2->GetTopLevelMesh(),Result,End,Start,Extent,TraceFlags))
		);
}

/**
 * will do a point check against ONLY this edge's related obstacle mesh for its navmesh(es). 
 * @param Result - output hit result describing the hit
 * @param Pt - point to check
 * @param Extent of box around point to check	
 * @param TraceFlags - traceflags to pass to PointCheck()
 * @return TRUE if there was no hit
 */
UBOOL FNavMeshEdgeBase::LimitedObstaclePointCheck( FCheckResult& Result, const FVector& Pt, const FVector& Extent, DWORD TraceFlags )
{
	FNavMeshPolyBase* Poly0 = GetPoly0();
	FNavMeshPolyBase* Poly1 = GetPoly1();

	UNavigationMeshBase* Mesh1 = NULL;
	UNavigationMeshBase* Mesh2 = NULL;

	if (Poly0 != NULL)
	{
		Mesh1 = Poly0->NavMesh;
	}

	if (Poly1 != NULL)
	{
		Mesh2 = Poly1->NavMesh;
	}

	return ( (!Mesh1 || Mesh1->GetObstacleMesh()->PointCheck(Mesh1->GetTopLevelMesh(),Result,Pt,Extent,TraceFlags)) && 
		(!Mesh2 || Mesh1 == Mesh2 || Mesh2->GetObstacleMesh()->PointCheck(Mesh2->GetTopLevelMesh(),Result,Pt,Extent,TraceFlags))
		);
}

/**
 *	Called from NavigationHandle.SuggestMovePreparation.  Allows this edge to run custom logic
 *  related to traversing this edge (e.g. jump, play special anim, etc.)
 * @param C - controller about to traverse this edge
 * @param out_Movept - point where this we should move to cross this edge (stuffed with non-special edge value to begin)
 * @return TRUE if this edge handled all movement to this edge, FALSE if the AI should still move to movept
 */
UBOOL FNavMeshEdgeBase::PrepareMoveThru( AController* C, FVector& out_MovePt)
{
	return FALSE;
}

/**
 * Allows edges which require special initialization to invalidate themselves when 
 * said initialization has not taken place
 * @param bAllowTopLevelEdgesWhenSubMeshPresent - when true edges which have been superseded by submesh edges will be allowed
 * NOTE: edges which link to polys which have a submesh will indicate they are invalid
 */
UBOOL FNavMeshEdgeBase::IsValid(UBOOL bAllowTopLevelEdgesWhenSubMeshPresent)
{
	if ( !bAllowTopLevelEdgesWhenSubMeshPresent )
	{
		// if we are linking to a poly which has a submesh, we are not valid! the dynamic edges will take care of it
		FNavMeshPolyBase* RESTRICT Poly1 = GetPoly1();
		FNavMeshPolyBase* RESTRICT Poly0 = GetPoly0();

		if ( ((Poly0 != NULL) && (Poly0->NumObstaclesAffectingThisPoly > 0)) || ((Poly1 != NULL) && (Poly1->NumObstaclesAffectingThisPoly > 0)) )
		{
			return FALSE;
		}
	}

	return !bPendingDelete;
}

/**
 *	Overridden to call SpecialMoveThruEdge on pawn
 * @param C - controller about to traverse this edge
 * @param out_Movept - point where this we should move to cross this edge (stuffed with non-special edge value to begin)
 * @return TRUE if this edge handled all movement to this edge, FALSE if the AI should still move to movept
 */
UBOOL FNavMeshSpecialMoveEdge::PrepareMoveThru( AController* C, FVector& out_MovePt )
{
	if( C != NULL && C->Pawn != NULL )
	{
		return C->Pawn->eventSpecialMoveThruEdge( EdgeType, MoveDir, out_MovePt, *MoveDest, *RelActor, RelItem, C->NavigationHandle );
	}

	return FALSE;
}

/**
 *	Overidden to adjust offset depending on the movedir
 * @param C - controller about to traverse this edge
 * @param out_Movept - point where this we should move to cross this edge (stuffed with non-special edge value to begin)
 * @return TRUE if this edge handled all movement to this edge, FALSE if the AI should still move to movept
 */
UBOOL FNavMeshMantleEdge::PrepareMoveThru( AController* C, FVector& out_MovePt )
{
	if( C != NULL && C->Pawn != NULL )
	{
		
		PointDistToEdge(C->Pawn->Location,TRUE,&out_MovePt);

		FNavMeshPolyBase* Poly0 = GetPoly0();

		FVector Extent = C->Pawn->GetCylinderExtent();

		if( Poly0 != NULL )
		{
			Poly0->AdjustPositionToDesiredHeightAbovePoly(out_MovePt,Extent.Z);
		}

		FVector Offset = *MoveDest - GetEdgeCenter(WORLD_SPACE);
		
		FCheckResult Hit(1.f);
		if( UNavigationHandle::StaticObstacleLineCheck(C,Hit,out_MovePt,out_MovePt+Offset,FVector(1.f),TRUE,NULL,NULL,TRACE_SingleResult) )
		{
			C->NavigationHandle->ComputeValidFinalDestination(out_MovePt);
		}
		else
		{
			out_MovePt = Hit.Location+Hit.Normal*FBoxPushOut(Hit.Normal,Extent);
		}



		// if this is a mantle up, just use movedest for dest 
		if(MoveDir > 0)
		{			
			return C->Pawn->eventSpecialMoveThruEdge( EdgeType, MoveDir, out_MovePt, *MoveDest, *RelActor, RelItem, C->NavigationHandle );
		}
		else
		{
			return C->Pawn->eventSpecialMoveThruEdge( EdgeType, MoveDir, out_MovePt, out_MovePt+Offset, *RelActor, RelItem, C->NavigationHandle );
		}
	}

	return FALSE;
}

/**
 *	Overidden to pass dropheight as movedir
 * @param C - controller about to traverse this edge
 * @param out_Movept - point where this we should move to cross this edge (stuffed with non-special edge value to begin)
 * @return TRUE if this edge handled all movement to this edge, FALSE if the AI should still move to movept
 */
UBOOL FNavMeshDropDownEdge::PrepareMoveThru( AController* C, FVector& out_MovePt )
{
	if( C != NULL && C->Pawn != NULL )
	{
		FVector Closest(0.f);
	
		FLOAT Radius = C->Pawn->GetCylinderExtent().X;
		FLOAT ArrivalOffset = 1.5f*Radius;
		const FVector PawnLoc = C->Pawn->Location;
		if( EffectiveEdgeLength < Radius*2.0f )
		{
			Closest = GetEdgeCenter();
		}
		else
		{
			FLOAT OffEdgeDist = PointDistToEdge(PawnLoc,WORLD_SPACE,&Closest);
		
			// find closest vert
			const FVector Vert0 = GetVertLocation(0,WORLD_SPACE);
			const FVector Vert1 = GetVertLocation(1,WORLD_SPACE);
			FLOAT DistSq0 = (Closest-Vert0).SizeSquared();
			FLOAT DistSq1 = (Closest-Vert1).SizeSquared();

			if( DistSq0 < DistSq1 )
			{
				if( DistSq0 < Radius*Radius )
				{
					Closest = Vert0 + (Vert1-Vert0).SafeNormal() * Radius;
				}
			}
			else
			{
				if( DistSq1 < Radius*Radius )
				{
					Closest = Vert1 + (Vert0-Vert1).SafeNormal() * Radius;
				}
			}
		}

		// We want to start the drop-down from the closest point on the edge
		out_MovePt = Closest;
		out_MovePt += C->GetEdgeZAdjust(this);

		if( (Closest-PawnLoc).Size2D() < ArrivalOffset )
		{
			FVector MoveDestPoint = out_MovePt;
			const FVector EdgePerp = GetEdgePerpDir(WORLD_SPACE);

			// figure out which side of the edge we're on, and get to the other side
			if( ((out_MovePt - PawnLoc).SafeNormal() | EdgePerp) > 0.f )
			{
				MoveDestPoint += EdgePerp * (ArrivalOffset+10.f);
			}
			else
			{
				MoveDestPoint -= EdgePerp * (ArrivalOffset+10.f);
			}

			return C->Pawn->eventSpecialMoveThruEdge( GetEdgeType(), appTrunc(DropHeight), out_MovePt, MoveDestPoint, NULL, 0, C->NavigationHandle );
		}
		else
		{
			return FALSE;
		}
	}

	return FALSE;
}

/**
 *	given one poly associated with this edge, will return the other
 * @param Poly - first poly (to find adjacent poly for)
 * @return - other poly
 */
FNavMeshPolyBase* FNavMeshEdgeBase::GetOtherPoly( FNavMeshPolyBase* Poly )
{
	return (Poly == GetPoly0()) ? GetPoly1() : GetPoly0();
}

/**
 * returns the cost of this edge for the passed object
 * @param Interface - the interface of the entity pathing along this edge
 * @param PreviousPoint - the point on the predecessor edge we are pathing from (and which we are computing costs from)
 * @param out_PathEdgePoint - the point we used along this edge to compute cost
 * @param SourcePoly - the source poly for this edge's consideration ( the poly we're coming from )
 * @return - cost of traversing this edge
 */
INT FNavMeshEdgeBase::CostFor( const FNavMeshPathParams& PathParams, const FVector& PreviousPoint,  FVector& out_PathEdgePoint, FNavMeshPolyBase* SourcePoly  )
{
	INT Dist = Max<INT>(appTrunc(PathParams.SearchExtent.X),appTrunc(PointDistToEdge(PreviousPoint, WORLD_SPACE, &out_PathEdgePoint)));
	FNavMeshPolyBase* OtherPoly = GetOtherPoly( SourcePoly );
	if( OtherPoly != NULL )
	{
		Dist += OtherPoly->GetTransientCost();
	}

	Dist += ExtraEdgeCost;

	APylon* Pylon = NavMesh->GetPylon();
	if(Pylon != NULL && Pylon->bNeedsCostCheck)
	{
		Pylon->CostFor(PathParams,PreviousPoint,out_PathEdgePoint,this,SourcePoly,Dist);
	}
	return Dist;
}

/**
 * (overidden to check both pylons for cost)
 * returns the cost of this edge for the passed object
 * @param Interface - the interface of the entity pathing along this edge
 * @param PreviousPoint - the point on the predecessor edge we are pathing from (and which we are computing costs from)
 * @param out_PathEdgePoint - the point we used along this edge to compute cost
 * @param SourcePoly - the source poly for this edge's consideration ( the poly we're coming from )
 * @return - cost of traversing this edge
 */
INT FNavMeshCrossPylonEdge::CostFor( const FNavMeshPathParams& PathParams, const FVector& PreviousPoint, FVector& out_PathEdgePoint, FNavMeshPolyBase* SourcePoly )
{
	INT Dist = FNavMeshEdgeBase::CostFor(PathParams,PreviousPoint,out_PathEdgePoint,SourcePoly);

	if( GetPoly0() && GetPoly1())
	{
		APylon* Py0 = GetPoly0()->NavMesh->GetPylon();
		APylon* Py1 = GetPoly1()->NavMesh->GetPylon();

		if(Py0->bNeedsCostCheck)
		{
			Py0->CostFor(PathParams,PreviousPoint,out_PathEdgePoint,this,SourcePoly,Dist);
		}
		else if(Py1->bNeedsCostCheck)
		{
			Py1->CostFor(PathParams,PreviousPoint,out_PathEdgePoint,this,SourcePoly,Dist);
		}
	}

	return Dist;
}


/**
* attempts to find edges which are close to co-linear forming a span
* @param out_Edges	- OUT the edges found in teh span
*/
FLOAT MinColinearDot = 0.98f;
#define MINDOT MinColinearDot
void FNavMeshEdgeBase::FindSpanEdges(TLookupMap<FNavMeshEdgeBase*>& Edges)
{
	FVector EdgeDir = (GetVertLocation(0) - GetVertLocation(1)).SafeNormal();

	// find all the edges that share each vertex
	static TArray<VERTID> EdgeVerts;
	EdgeVerts.Reset(2);
	EdgeVerts.AddItem(Vert0);
	EdgeVerts.AddItem(Vert1);
	for(INT EdgeVertIdx=0;EdgeVertIdx<Min<INT>(EdgeVerts.Num(),2);EdgeVertIdx++)
	{
		FMeshVertex& Vert = NavMesh->Verts(EdgeVerts(EdgeVertIdx));
		
		for(INT ContainingPolyIdx=0;ContainingPolyIdx<Vert.GetNumContainingPolys();ContainingPolyIdx++)
		{
			FNavMeshPolyBase* CurPoly = Vert.GetContainingPolyAtIdx(ContainingPolyIdx,NavMesh);
			
			for(INT PolyEdgeIdx=0;PolyEdgeIdx<CurPoly->PolyEdges.Num();PolyEdgeIdx++)
			{
				FNavMeshEdgeBase* CurEdge = CurPoly->NavMesh->GetEdgeAtIdx(CurPoly->PolyEdges(PolyEdgeIdx));
				
				if(CurEdge->IsValid() && CurEdge->HasVert(EdgeVerts(EdgeVertIdx)))
				{			
					FVector CurEdgeDir = (CurEdge->GetVertLocation(0) - CurEdge->GetVertLocation(1)).SafeNormal();

					FLOAT Dot = Abs<FLOAT>(CurEdgeDir | EdgeDir);
					if(Dot > MINDOT)
					{
						if(Edges.Find(CurEdge) == NULL)
						{
							Edges.AddItem(CurEdge);
							CurEdge->FindSpanEdges(Edges);
						}
					}
				}
			}
		}
	
	}
}

/*
* uses the midpoint of the found span to determine the location furthest away from boundaries
* @return the location furthest away from boundaries along the edge
*/
FVector FNavMeshEdgeBase::FindMultiEdgeSpanMidPoint()
{
	TLookupMap<FNavMeshEdgeBase*> SpanEdges;
	SpanEdges.AddItem(this);
	FindSpanEdges(SpanEdges);

	FVector Ctr(0.f);
	for(INT EdgeIdx=0;EdgeIdx<SpanEdges.Num();EdgeIdx++)
	{
		Ctr+=SpanEdges(EdgeIdx)->GetEdgeCenter(LOCAL_SPACE);
	}
	Ctr /= SpanEdges.Num();

	return Ctr;
}

/**
 * returns whether or not this edge supports the searcher	
 * @param PathParams - parameter struct containing information about the searching entity
 * @param CurPoly - poly we're coming from (e.g. the poly we're trying to leave)
 * @param PredecessorEdge - the edge we're coming to this edge from 
 * @return TRUE if this edge supports the searcher
 */
UBOOL FNavMeshEdgeBase::Supports( const FNavMeshPathParams& PathParams,
							FNavMeshPolyBase* CurPoly,
							FNavMeshEdgeBase* PredecessorEdge)
{
	const FVector Extent = PathParams.SearchExtent;
	const FLOAT Rad = Max<FLOAT>(Extent.X,Extent.Y);

	// if the edge is wide enough itself then we know we're good to go 
	if ( EffectiveEdgeLength+KINDA_SMALL_NUMBER <= Rad )
	{

		//GWorld->GetWorldInfo()->DrawDebugLine(GetVertLocation(0),GetVertLocation(1),255,0,0,TRUE);
		return FALSE;
	}


	// If height doesn't fit
	FNavMeshPolyBase* OtherPoly = GetOtherPoly(CurPoly);
	if( OtherPoly != NULL && OtherPoly->GetPolyHeight() <= (Extent.Z*2.f) )
	{
		return FALSE;
	}
	//GWorld->GetWorldInfo()->DrawDebugLine(GetVertLocation(0),GetVertLocation(1),0,255,0,TRUE);

	if( PredecessorEdge != NULL )
	{
		// check for parralax space.. need to see if the trajectory from the previous edge to this one falls within curPoly, if not
		// bail on the edge and let the dude go around through an additional poly

		check(PredecessorEdge != this);
		if( ! PredecessorEdge->SupportsMoveToEdge(PathParams,this,CurPoly) )
		{
			return FALSE;
		}		
	}

	return TRUE;
	
}

/**
 * NOTE: this will return only VALID edges ( static edges which are marked invalid will be skipped )
 * gets a list of all edges in the same group as this edge coming from the designated srcpoly
 * @param SrcPoly - poly we want the group for (all edges from this poly to the poly this edge points to that match this edge's group ID)
 * @param out_EdgesInGroup - the edges in the group
 */
void FNavMeshEdgeBase::GetAllStaticEdgesInGroup(FNavMeshPolyBase* SrcPoly, TArray<FNavMeshEdgeBase*>& out_EdgesInGroup)
{
	FNavMeshPolyBase* Poly0 = GetPoly0();
	FNavMeshPolyBase* Poly1 = GetPoly1();
	
	if( SrcPoly == NULL || Poly0 == NULL || Poly1 == NULL ) 
	{
		return;
	}

	// MAXBYTE indicates we are not in a group, so add ourselves and exit
	if( EdgeGroupID == MAXBYTE )
	{
		out_EdgesInGroup.AddItem(this);
		return;
	}

	for( INT EdgeIdx = 0; EdgeIdx < SrcPoly->GetNumEdges(); ++EdgeIdx )
	{
		FNavMeshEdgeBase* CurEdge = SrcPoly->GetEdgeFromIdx(EdgeIdx,SrcPoly->NavMesh,TRUE);
		check(CurEdge!=NULL);

		if(CurEdge->EdgeGroupID == EdgeGroupID )
		{
			FNavMeshPolyBase* CurPoly0 = CurEdge->GetPoly0();
			FNavMeshPolyBase* CurPoly1 = CurEdge->GetPoly1();

			if( (CurPoly0 == Poly0 && CurPoly1 == Poly1) ||
				(CurPoly0 == Poly1 && CurPoly1 == Poly0) )
			{
				out_EdgesInGroup.AddItem(CurEdge);
			}
		}
	}

}

/**
 * gets a list of all edges in the same group as this edge coming from the designated srcpoly
 * @param SrcPoly - poly we want the group for (all edges from this poly to the poly this edge points to that match this edge's group ID)
 * @param out_EdgesInGroup - the edges in the group
 */
void FNavMeshEdgeBase::GetAllEdgesInGroup(FNavMeshPolyBase* SrcPoly, TArray<FNavMeshEdgeBase*>& out_EdgesInGroup)
{
	FNavMeshPolyBase* RESTRICT Poly0 = GetPoly0();
	FNavMeshPolyBase* RESTRICT Poly1 = GetPoly1();
	
	if( SrcPoly == NULL || Poly0 == NULL || Poly1 == NULL ) 
	{
		return;
	}

	// MAXBYTE indicates we are not in a group, so add ourselves and exit
	if( EdgeGroupID == MAXBYTE )
	{
		out_EdgesInGroup.AddItem(this);
		return;
	}

	FNavMeshEdgeBase* CurEdge = NULL;  
	for( INT EdgeIdx = 0; EdgeIdx < SrcPoly->GetNumEdges(); ++EdgeIdx )
	{
		CurEdge = SrcPoly->GetEdgeFromIdx(EdgeIdx);  

		if(CurEdge != NULL &&  CurEdge->EdgeGroupID == EdgeGroupID )
		{
			FNavMeshPolyBase* RESTRICT CurPoly0 = CurEdge->GetPoly0();
			FNavMeshPolyBase* RESTRICT CurPoly1 = CurEdge->GetPoly1();

			if( (CurPoly0 == Poly0 && CurPoly1 == Poly1) ||
				(CurPoly0 == Poly1 && CurPoly1 == Poly0) )
			{
				out_EdgesInGroup.AddItem(CurEdge);
			}
		}
	}

}

void FNavMeshEdgeBase::Cache(class UNavigationMeshBase* Mesh)
{
	NavMesh = Mesh;
	Mesh->EdgePtrs.AddItem(this);
}

void FNavMeshCrossPylonEdge::Cache(class UNavigationMeshBase* Mesh)
{
	FNavMeshEdgeBase::Cache(Mesh);
	bIsCrossPylon=TRUE;
	Mesh->CrossPylonEdges.AddItem(this);
}

/**
 * (overidden from parent to check mantle specific stuff)
 * @param PathParams - parameter struct containing information about the searching entity
 * @param CurPoly - poly we're coming from (e.g. the poly we're trying to leave)
 * @param PredecessorEdge - the edge we're coming to this edge from 
 * @return TRUE if this edge supports the searcher
 */
UBOOL FNavMeshMantleEdge::Supports(const FNavMeshPathParams& PathParams,
								   FNavMeshPolyBase* CurPoly,
								   FNavMeshEdgeBase* PredecessorEdge)
{
	if( !PathParams.bCanMantle )
	{
		return FALSE;
	}

	// if slot/link is disabled, then no it's not supported!
	ACoverLink* Link = Cast<ACoverLink>(*RelActor);
	if(Link != NULL)
	{
		AController* Controller = Cast<AController>(PathParams.Interface->GetUObjectInterfaceInterface_NavigationHandle());
		APawn* AskingPawn = (Controller) ? Controller->Pawn : NULL;
		if ( ! Link->IsValidClaim(AskingPawn,RelItem,TRUE,TRUE) )
		{
			return FALSE;
		}
	}
	else
	{
		return FALSE;
	}

	if( PathParams.bNeedsMantleValidityTest )
	{
		return PathParams.Interface->CheckMantleValidity(this);
	}

	return TRUE;
}

/** 
 * (overidden to return closest point on segment)
 * @param EntityRadius - the radius of the entity we need to build an edge position for
 * @param InfluencePosition - the position we should try and get close to with our edge position
 * @param EntityPosition    - the position the entity will be in when it starts moving toward this edge
 */
FVector FNavMeshMantleEdge::GetEdgeDestination(const FNavMeshPathParams& PathParams,
												FLOAT EntityRadius,
												const FVector& InfluencePosition,
												const FVector& EntityPosition,
												UNavigationHandle* Handle,
												UBOOL bFirstPass )
{
	FVector ClosestPt(0.f);

	PointDistToSegment(InfluencePosition,GetVertLocation(0,WORLD_SPACE),GetVertLocation(1,WORLD_SPACE),ClosestPt);
	return ClosestPt+PathParams.Interface->GetEdgeZAdjust(this);
}

/**
 * wrapper each subclass should override which should simply mirror AddDynamicCrossPylonEdge and pass the proper type
 * to the template (e.g. AddOneWayCrossPylonEdgeToMesh<FNavMeshMantleEdge>(EdgeStart, EdgeEnd, ConnectedPolys, &EdgePtr );
 */
FNavMeshSpecialMoveEdge* FNavMeshMantleEdge::AddTypedEdgeForObstacleReStitch(UNavigationMeshBase* NavMesh, const FVector& StartPt, const FVector& EndPt, TArray<FNavMeshPolyBase*>& ConnectedPolys)
{
	TArray<FNavMeshMantleEdge*> ReturnEdges;
	NavMesh->AddDynamicCrossPylonEdge<FNavMeshMantleEdge>(StartPt, EndPt, ConnectedPolys, EffectiveEdgeLength,EdgeGroupID, TRUE, &ReturnEdges );
	if( ReturnEdges.Num() > 0)
	{
		return ReturnEdges(0);
	}
	return NULL;
}

/**
 * (overidden from parent to check coverslip specific stuff)
 * @param PathParams - parameter struct containing information about the searching entity
 * @param CurPoly - poly we're coming from (e.g. the poly we're trying to leave)
 * @param PredecessorEdge - the edge we're coming to this edge from 
 * @return TRUE if this edge supports the searcher
 */
UBOOL FNavMeshCoverSlipEdge::Supports(const FNavMeshPathParams& PathParams,
									  FNavMeshPolyBase* CurPoly,
									  FNavMeshEdgeBase* PredecessorEdge)
{
	ACoverLink* Link = NULL;
	if(*RelActor != NULL)
	{
		Link = (ACoverLink*)(*RelActor);
	}
	if( PathParams.Interface != NULL && !PathParams.Interface->CanCoverSlip(Link,RelItem ) )
	{
		return FALSE;
	}

	return TRUE;
}

INT FNavMeshCoverSlipEdge::CostFor( const FNavMeshPathParams& PathParams,
					const FVector& PreviousPoint,
					FVector& out_PathEdgePoint,
					FNavMeshPolyBase* SourcePoly )
{
	INT SuperVal = FNavMeshSpecialMoveEdge::CostFor(PathParams,PreviousPoint,out_PathEdgePoint,SourcePoly);

	FLOAT NewCost = (FLOAT)SuperVal * 0.25f;
	out_PathEdgePoint = GetEdgeCenter();
	return appTrunc(NewCost);
}

/**
 * callback after a path for an AI is generated, allowing each edge to modify things on the handle if necessary
 * @param Handle - the handle whose path was just constructed
 * @param PathIdx - index into the handle path array that points to this edge
 * @return TRUE if we modified the pathcache
 */
UBOOL FNavMeshCoverSlipEdge::PathConstructedWithThisEdge(UNavigationHandle* Handle, INT PathIdx)
{
	// if we are in the pathcache it's because the AI was already in the coverlink associated with this coverslip.
	// clip off everything before the coverslip so we move within the link to get to the slip
	if( PathIdx != 0 )
	{
		return Handle->PathCache_RemoveIndex(0,PathIdx);
	}

	return FALSE;
}

/**
 * wrapper each subclass should override which should simply mirror AddDynamicCrossPylonEdge and pass the proper type
 * to the template (e.g. AddOneWayCrossPylonEdgeToMesh<FNavMeshMantleEdge>(EdgeStart, EdgeEnd, ConnectedPolys, &EdgePtr );
 */
FNavMeshSpecialMoveEdge* FNavMeshCoverSlipEdge::AddTypedEdgeForObstacleReStitch(UNavigationMeshBase* NavMesh, const FVector& StartPt, const FVector& EndPt, TArray<FNavMeshPolyBase*>& ConnectedPolys)
{
	TArray<FNavMeshCoverSlipEdge*> ReturnEdges;
	NavMesh->AddDynamicCrossPylonEdge<FNavMeshCoverSlipEdge>(StartPt, EndPt, ConnectedPolys, EffectiveEdgeLength, EdgeGroupID,TRUE, &ReturnEdges );
	if( ReturnEdges.Num() > 0)
	{
		return ReturnEdges(0);
	}
	return NULL;
}
	/**
	 *	Called from NavigationHandle.SuggestMovePreparation.  Allows this edge to run custom logic
	 *  related to traversing this edge (e.g. jump, play special anim, etc.)
	 * @param C - controller about to traverse this edge
	 * @param out_Movept - point where this we should move to cross this edge (stuffed with non-special edge value to begin)
	 * @return TRUE if this edge handled all movement to this edge, FALSE if the AI should still move to movept
	 */
UBOOL FNavMeshCoverSlipEdge::PrepareMoveThru( class AController* C, FVector& out_MovePt )
{

	if( C != NULL && C->Pawn != NULL )
	{
		return C->Pawn->eventSpecialMoveThruEdge( GetEdgeType(), 0, out_MovePt, out_MovePt, RelActor,RelItem, C->NavigationHandle );
	}

	return FALSE;
}

 /** 
  * allows this edge to override getnextmovelocation
  * @param out_Dest - out param describing the destination we should strive for
  * @param Arrivaldistance - distance within which we will be considered 'arrived' at a point
  * @param out_ReturnStatus - value GetNextMoveLocation should return
  * @return TRUE if this edge is overriding getnextmovelocation
  * COVERSLIP: since coverslip edges are always the first in the pathcache, they might not be in the same poly as we are
  *            but they will always be from the same coverlink.. thus we need to override getnextmovelocation so that it doesn't bail
  *            when it discovers we're not in the poly this edge is attached to
  */
 UBOOL FNavMeshCoverSlipEdge::OverrideGetNextMoveLocation(UNavigationHandle* Handle, FVector& out_Dest, FLOAT ArrivalDistance, UBOOL& out_ReturnStatus )
 {
	// tell GNML to say everything is fine :) 
	out_ReturnStatus = TRUE;
	Handle->CurrentEdge = this;

	// note! it's up to the script to remove this edge from the cache so the bot can progress, cuz the normal mechanism for clipping
	// traversed edges is being circumvented
	return TRUE;
 }

/**
 * (overidden from parent to check dropdown specific stuff)
 * @param PathParams - parameter struct containing information about the searching entity
 * @param CurPoly - poly we're coming from (e.g. the poly we're trying to leave)
 * @param PredecessorEdge - the edge we're coming to this edge from
 * @return TRUE if this edge supports the searcher
 */
UBOOL FNavMeshDropDownEdge::Supports(const FNavMeshPathParams& PathParams,
									 FNavMeshPolyBase* CurPoly,
									 FNavMeshEdgeBase* PredecessorEdge)
{
	if(!FNavMeshCrossPylonEdge::Supports(PathParams,CurPoly,PredecessorEdge))
	{
		return FALSE;
	}

	return PathParams.MaxDropHeight > DropHeight;
}

// PATHOBJECT EDGE STUFF
IMPLEMENT_EDGE_CLASS(FNavMeshPathObjectEdge)


FString FNavMeshEdgeBase::GetDebugText()
{
	if( bPendingDelete )
	{
		return FString(TEXT("! Edge pending delete"));
	}

	if( !IsValid() || !GetPoly0() || !GetPoly1() )
	{
		return FString(TEXT("! Edge not fully loaded!"));
	}
	return FString::Printf(TEXT("(%s) -- (Len:%.2f EffecLen:%.2f Poly0Height:%.2f Poly1Height:%2.f EdgeGroupID:%i) Ctr:%s"),*GetClassSpecificDebugText(),GetEdgeLength(),EffectiveEdgeLength,GetPoly0()->GetPolyHeight(),GetPoly1()->GetPolyHeight(),EdgeGroupID,*GetEdgeCenter().ToString()); 
}
FString FNavMeshEdgeBase::GetClassSpecificDebugText() 
{ 
	return FString(TEXT("FNavMeshEdgeBase")); 
}


/**
 * this function is called after a poly linked to this edge is replaced with a submesh for a pathobstacle
 * allows special edges to have a chance to add extra data after the mesh is split
 * @param Poly - the poly that was just disabled and replaced with a submesh
 * @param NewSubMesh - the submesh that now represents the poly
 * @param bFromBackRef - are we adding edges from a backreference? (e.g. B just got rebuild, and we're doing edge adds for an edge that points from A->B)
 */
void FNavMeshPathObjectEdge::PostSubMeshUpdateForOwningPoly(FNavMeshPolyBase* Poly, UNavigationMeshBase* New_SubMesh, UBOOL bFromBackRef/*=FALSE*/)
{
	IInterface_NavMeshPathObject* PO = (*PathObject!=NULL) ? InterfaceCast<IInterface_NavMeshPathObject>(*PathObject) : NULL;
	if(PO!=NULL)
	{
		PO->PostSubMeshUpdateForOwningPoly(this,Poly,New_SubMesh);
	}

	// 
}

/**
 * (overidden to ask the PO for cost)
 * @param Interface - the interface of the entity pathing along this edge
 * @param PreviousPoint - the point on the predecessor edge we are pathing from (and which we are computing costs from)
 * @param out_PathEdgePoint - the point we used along this edge to compute cost
 * @param SourcePoly - the source poly for this edge's consideration ( the poly we're coming from )
 * @return - cost of traversing this edge
 */
INT FNavMeshPathObjectEdge::CostFor( const FNavMeshPathParams& PathParams,
									const FVector& PreviousPoint, FVector& out_PathEdgePoint, FNavMeshPolyBase* SourcePoly )
{
	IInterface_NavMeshPathObject* PO = (*PathObject!=NULL) ? InterfaceCast<IInterface_NavMeshPathObject>(*PathObject) : NULL;
	if(PO==NULL)
	{
		return UCONST_BLOCKEDPATHCOST;
	}
	

	return PO->CostFor(PathParams,PreviousPoint,out_PathEdgePoint,this, SourcePoly);
}

FArchive& FNavMeshPathObjectEdge::Serialize( FArchive& Ar )
{
	FNavMeshCrossPylonEdge::Serialize(Ar);

	Ar << PathObject;
	Ar << InternalPathObjectID;
	return Ar;
}
void FNavMeshPathObjectEdge::GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel, UBOOL bIsDynamic)
{
	FNavMeshCrossPylonEdge::GetActorReferences(ActorRefs,bIsRemovingLevel,bIsDynamic);

	if( (bIsRemovingLevel && PathObject.Actor != NULL) || 
		(!bIsRemovingLevel && PathObject.Actor == NULL))
	{
		ActorRefs.AddItem(&PathObject);
	}
}

/**
 * (overidden to ask the PO)
 * @param PathParams - parameter struct containing information about the searching entity
 * @param CurPoly - poly we're coming from (e.g. the poly we're trying to leave)
 * @param PredecessorEdge - the edge we're coming to this edge from
 * @return TRUE if this edge supports the searcher
 */
UBOOL FNavMeshPathObjectEdge::Supports(const FNavMeshPathParams& PathParams,
									   FNavMeshPolyBase* CurPoly,
									   FNavMeshEdgeBase* PredecessorEdge)
{
	IInterface_NavMeshPathObject* PO = (*PathObject!=NULL) ? InterfaceCast<IInterface_NavMeshPathObject>(*PathObject) : NULL;
	if(PO==NULL)
	{
		return FALSE;
	}

	return PO->Supports(PathParams,CurPoly,this,PredecessorEdge);
}

/** (overidden to ask the PO) */
UBOOL FNavMeshPathObjectEdge::PrepareMoveThru( AController* C, FVector& out_MovePt )
{
	IInterface_NavMeshPathObject* PO = (*PathObject!=NULL) ? InterfaceCast<IInterface_NavMeshPathObject>(*PathObject) : NULL;
	if(PO==NULL)
	{
		return FALSE;
	}

	IInterface_NavigationHandle* NavHandleInt = InterfaceCast<IInterface_NavigationHandle>(C);


	return PO->PrepareMoveThru(NavHandleInt,out_MovePt,this);	
}

/** (overidden to ask the PO) */
FVector FNavMeshPathObjectEdge::GetEdgeDestination( const FNavMeshPathParams& PathParams,
													FLOAT EntityRadius,
													const FVector& InfluencePosition,
													const FVector& EntityPosition,
													UNavigationHandle* Handle,
													UBOOL bFirstPass )
{
	IInterface_NavMeshPathObject* PO = (*PathObject!=NULL) ? InterfaceCast<IInterface_NavMeshPathObject>(*PathObject) : NULL;
	FVector NewDest(0.f);
	if(PO==NULL || ! PO->GetEdgeDestination(PathParams,EntityRadius,InfluencePosition,EntityPosition,NewDest,this,Handle))
	{
		return FNavMeshCrossPylonEdge::GetEdgeDestination(PathParams,EntityRadius,InfluencePosition,EntityPosition,Handle, bFirstPass );
	}

	return NewDest;
}

/** (overidden to ask the PO) */
UBOOL FNavMeshPathObjectEdge::AllowMoveToNextEdge(FNavMeshPathParams& PathParams, UBOOL bInPoly0, UBOOL bInPoly1)
{
	IInterface_NavMeshPathObject* PO = (*PathObject!=NULL) ? InterfaceCast<IInterface_NavMeshPathObject>(*PathObject) : NULL;
	if (PO != NULL)
	{
		return PO->AllowMoveToNextEdge(PathParams,bInPoly0,bInPoly1);
	} 
	return TRUE;
}

/**
 * called when levels are being saved and cross level refs need to be cleared
 */
UBOOL FNavMeshPathObjectEdge::ClearCrossLevelReferences()
{
	UBOOL bSuperRetVal = FNavMeshCrossPylonEdge::ClearCrossLevelReferences();

	APylon* MyPylon = NavMesh->GetPylon();
	if( MyPylon == NULL )
	{
		return bSuperRetVal;
	}

	if( PathObject.Actor != NULL )
	{
		if(PathObject->GetOutermost() != MyPylon->GetOutermost())
		{
			FGuid* pGuid = PathObject.Actor->GetGuid();
			if( pGuid != NULL )
			{
				PathObject.Guid = *pGuid;
			}
			else
			{
				debugf(TEXT("WARNING: FNavMeshPathObjectEdge::ClearCrossLevelReferences() FAILED TO GET GUID FOR %s"), *PathObject.Actor->GetName() );
			}
			PathObject.Actor = NULL;			
			bSuperRetVal = TRUE;
		}
	}

	return bSuperRetVal;
}


// END PO EDGE STUFF


FLOAT TriangleArea2(FVector A, FVector B, FVector C)
{
	return ((B.X - A.X) * (C.Y - A.Y) - (C.X - A.X) * (B.Y - A.Y));
}

FLOAT TriangleArea2_3D(FVector A, FVector B, FVector C)
{
	return ((A-B) ^ (C-B)).Size();
}

TArray<FNavMeshPolyBase*> FNavMeshPolyBase::TransientCostedPolys;

FNavMeshPolyBase::FNavMeshPolyBase( UNavigationMeshBase* Mesh, const TArray<WORD>& inPolyIndices, FLOAT InPolyHeight) :
	FNavMeshObject( Mesh ),
	PolyVerts(inPolyIndices),
	TransientCost(0),
	BorderListNode(NULL),
	NumObstaclesAffectingThisPoly(0)
{
	PolyBuildLoc = FVector(0.f);
	PolyCenter = FVector(0.f);
	BoxBounds.Init();

	PolyHeight = InPolyHeight;

	FVector PolyUp(0.f);
	RecalcAfterVertChange(&PolyUp);

	checkSlow(!PolyNormal.IsNearlyZero());
	checkSlow(CalcArea() > 0.f);
	if( PolyNormal.IsNearlyZero() || CalcArea() < KINDA_SMALL_NUMBER )
	{
		warnf(NAME_Error,TEXT("Tried to add degenerate geometry! (0 area triangle, zero length normal) (%s).. Abort"), *Mesh->GetPylon()->GetName());
	}

	// if no height was passed, calculate a height right now
	if(PolyHeight < 0.f)
	{
		FCheckResult Result;
		FVector Extent = BoxBounds.GetExtent();
		Extent.Z = NAVMESHGEN_ENTITY_HALF_HEIGHT;
		FVector PCtr = GetPolyCenter();
		FVector Ceiling = Mesh->GetPylon()->FindCeiling(PCtr,Result,FPathBuilder::GetScout(),PolyUp,Extent);
		PolyHeight = (Ceiling-PCtr).Size();
	}


	for( INT Idx = 0; Idx < PolyVerts.Num(); Idx++ )
	{
		FVector Vert = NavMesh->GetVertLocation(PolyVerts(Idx),LOCAL_SPACE);
		// expand bounds compensating for poly computed height
		BoxBounds += Vert + PolyHeight * PolyUp; 
	}

	//checkSlowish(GetPolyHeight() > NAVMESHGEN_ENTITY_HALF_HEIGHT);
	/*
	if(GetPolyHeight() < NAVMESHGEN_ENTITY_HALF_HEIGHT)
	{
		GWorld->GetWorldInfo()->DrawDebugLine(GetPolyCenter(),GetPolyCenter()+FVector(0.f,0.f,300.f),255,0,0,TRUE);
	}*/
}

/**
 *	called after vertexes for this polygon change and poly params need to be recalculated (e.g. norm, center, etc)
 * @param Direction to be considered Up (optional, use normal if none specified)
 */
void FNavMeshPolyBase::RecalcAfterVertChange(FVector* inPolyUp)
{
	PolyCenter = CalcCenter(LOCAL_SPACE);
	PolyNormal = CalcNormal(LOCAL_SPACE);

	FVector WS_PolyNorm = GetPolyNormal(WORLD_SPACE);
	UBOOL bUsePolyUp = WS_PolyNorm.Z < NAVMESHGEN_MIN_WALKABLE_Z;

	FVector PolyUp(0.f, 0.f, 1.f);
	if ( bUsePolyUp )
	{
		PolyUp = PolyNormal;
	}
	if(inPolyUp != NULL)
	{
		*inPolyUp = PolyUp;
	}

	for( INT Idx = 0; Idx < PolyVerts.Num(); Idx++ )
	{
		FVector Vert = NavMesh->GetVertLocation(PolyVerts(Idx),LOCAL_SPACE);
		BoxBounds += Vert - ExpansionPolyBoundsDownOffset * PolyUp; 
	}
}

FNavMeshPolyBase::~FNavMeshPolyBase()
{
	if (NavMesh != NULL)
	{
		for(INT Idx=0;Idx<PolyVerts.Num();Idx++)
		{
			NavMesh->Verts(PolyVerts(Idx)).ContainingPolys.RemoveItemSwap(this);
		}
	}

	if (TransientCost != 0)
	{
		TransientCostedPolys.RemoveItem(this);
	}
}

/** (static) called to reset TransientCost on all entries in the TransientCostedPolys list */
void FNavMeshPolyBase::ClearTransientCosts()
{
	for (INT i = 0; i < TransientCostedPolys.Num(); i++)
	{
		TransientCostedPolys(i)->TransientCost = 0;
	}
	TransientCostedPolys.Reset();
}

void FNavMeshPolyBase::DebugLog( INT Idx )
{
	debugf(TEXT("Poly %d"), Idx );
	debugf(TEXT("\tCenter %s"), *PolyCenter.ToString());
	debugf(TEXT("\tNormal %s"), *PolyNormal.ToString());
	for( INT VertIdx = 0; VertIdx < PolyVerts.Num(); VertIdx++ )
	{
		debugf(TEXT("\t\t%d VertIdx: %d"), VertIdx, PolyVerts(VertIdx) );
	}
}

void FNavMeshPolyBase::DebugLogVerts( TCHAR* PolyName )
{
	debugf(TEXT("Log poly: %s Convex: %s"), PolyName, NavMesh->IsConvex(PolyVerts)?TEXT("TRUE"):TEXT("FALSE") );
	for( INT I = 0; I < PolyVerts.Num(); I++ )
	{
		debugf(TEXT("\tVert%d: %d"),I,PolyVerts(I));
	}
}


/**
 * this will search the vertex hash for a vert that is clsoe to the passed location
 * @param InVec - location for new vert
 * @param bWorldSpace - (optional) TRUE if the incoming vert is in world space
 * @param MaxZDelta - (optional) max vertical distance between verts to be considered the 'same'
 * @param bUseTopMatchingVert - (optional) when TRUE the vert that is highest will be used to break vertical ties
 * @param MaxDelta - (optional) max distance between verts to be considered the 'same'
 * @return VERTID of vert found (or MAXVERTID if none is found)
 */
VERTID UNavigationMeshBase::FindVert( const FVector& inVec, UBOOL bWorldSpace/*=TRUE*/, FLOAT MaxZDelta/*=-1.0f*/, UBOOL bUseTopMatchingVert/*=FALSE*/, FLOAT MaxDelta/*=-1.0f */ )
{
	FMeshVertex inV = FMeshVertex( (bWorldSpace) ? FVector(W2LTransformFVector(inVec)) : inVec );


	// Loop through all verts... 
	if(VertHash == NULL)
	{
		VertHash = new FVertHash();
	}

	MaxZDelta = (MaxZDelta > -1.0f) ? MaxZDelta : NAVMESHGEN_MAX_STEP_HEIGHT;

	static TArray<VERTID> FoundVerts;
	FoundVerts.Reset(FoundVerts.Num());
	VertHash->MultiFind(inV,FoundVerts);
	// If any of the passed in verts are close enough to existing vert just use the existing vert index
	FLOAT BestDistSq = BIG_NUMBER;
	VERTID BestVertIdx = MAXVERTID;
	for( INT Idx = 0; Idx < FoundVerts.Num(); Idx++ )
	{
		VERTID FoundVertIdx = FoundVerts(Idx);
		// NOTE: all of this math is in local space
		FLOAT DeltaZ = Abs<FLOAT>(Verts(FoundVertIdx).Z-inV.Z);
		if( DeltaZ < MaxZDelta )
		{
			FLOAT RealDistSq = (Verts(FoundVertIdx)-inV).SizeSquared();
			// when bUseTopMatchingVert is TRUE, we want the one with the highest Z, that's also within stepheight
			FLOAT EffectiveDistSq = (bUseTopMatchingVert) ? -Verts(FoundVertIdx).Z : RealDistSq;			
			if(EffectiveDistSq < BestDistSq && ( MaxDelta < 0.f || RealDistSq < MaxDelta*MaxDelta ) )
			{
				BestDistSq = EffectiveDistSq;
				BestVertIdx = FoundVertIdx;
			}
		}
	}

	return BestVertIdx;
}
/**
 *	this will add a new mesh vertex at the supplied location, or return the ID of one existing already which is close enough to be considered the same
 * @param InVec - location for new vert
 * @param bWorldSpace - (optional) TRUE if the incoming vert is in world space
 * @param MaxZDelta - (optional) max vertical distance between verts to be considered the 'same'
 * @param bUseTopMatchingVert - (optional) when TRUE the vert that is highest will be used to break vertical ties
 * @param MaxDelta - (optional) max distance between verts to be considered the 'same'
 * @return VERTID of either new vert or the vert which was found to be close enough
 */
VERTID UNavigationMeshBase::AddVert( const FVector& inVec, UBOOL bWorldSpace, FLOAT MaxZDelta/*=-1.0f*/, UBOOL bUseTopMatchingVert/*=FALSE*/, FLOAT MaxDelta/*=-1.0f*/  )
{

	checkSlow(!inVec.ContainsNaN());

	FMeshVertex inV = FMeshVertex( (bWorldSpace) ? FVector(W2LTransformFVector(inVec)) : inVec );

	VERTID TheVert = FindVert(inV,LOCAL_SPACE,MaxZDelta,bUseTopMatchingVert,MaxDelta);
	if( TheVert != MAXVERTID )
	{
		Verts(TheVert).Z = Max<FLOAT>(Verts(TheVert).Z,inV.Z);
	}
	else
	{
		// Otherwise, add the vert to the pool
		TheVert = Verts.AddItem( inV );
		VERTID AddedVal = VertHash->Add(inV,TheVert);
		//debugf(TEXT("%s, typehash is %u, addedval: %u"),*inV.ToString(),GetTypeHash(inV),AddedVal);
	}


#if !FINAL_RELEASE
	if( Verts.Num() > MAXVERTID )
	{
		warnf(NAME_Error,TEXT("%s EXCEEDED MAX NUMBER OF VERTS ALLOWED!\nTry using multiple pylons with smaller bounds"), *GetPylon()->GetFullName());
	}
#endif

	return TheVert;
}

/** 
* Add a dynamic (runtime) vert to the pool -- return the index of the new vert
* Might be slow because we have no vert hash at this point, uses octree check
* @param inV		  -	world space position of the new vertex to be added
* @param bWorldSpace - (optional) if TRUE vert location will be treated as worldspace, defaults to TRUE
* @return		-	the ID of the added (or already existing) vertex
*/
VERTID UNavigationMeshBase::AddDynamicVert( const FVector& inV, UBOOL bWordlSpace )
{
	// look for normal vert first
	VERTID NormalVert = FindVert(inV,bWordlSpace);
	if( NormalVert != MAXVERTID )
	{
		return NormalVert;
	}

	FVector LS_Vert = (bWordlSpace) ? FVector(W2LTransformFVector(inV)) : inV;

	TArray<VERTID> FoundVerts;
	GetAllVertsNearPoint(LS_Vert,FVector(3.f),FoundVerts);
	
	// If any of the passed in verts are close enough to existing vert just use the existing vert index
	for( INT Idx = 0; Idx < FoundVerts.Num(); Idx++ )
	{
		VERTID FoundVertIdx = FoundVerts(Idx);
		// NOTE: all of this math should be in local space
		if( Abs<FLOAT>(Verts(FoundVertIdx).Z-LS_Vert.Z) < NAVMESHGEN_MAX_STEP_HEIGHT)
		{
			Verts(FoundVertIdx).Z = Max<FLOAT>(Verts(FoundVertIdx).Z,LS_Vert.Z);
			return FoundVertIdx;
		}
	}
	// Otherwise, add the vert to the pool
	VERTID Idx = Verts.AddItem( LS_Vert );

	checkSlowish(Idx != MAXVERTID);

	return Idx;
}

/**
 * @param VertId      - the index of the vertex you want the location of
 * @param bWorldSpace - if TRUE, value will be in world space, otherwise will be in local space
 * @param bOneWay	  - TRUE if we should only add a link to the added edge from poly0->poly1, and not Poly1->Poly0
 * @return - the location of the vertex
 */
FVector UNavigationMeshBase::GetVertLocation(VERTID VertId, UBOOL bWorldSpace) const
{
	const FMeshVertex& Vertex = Verts(VertId);
	if(!bWorldSpace)
	{
		return Vertex;
	}
	else
	{
		return L2WTransformFVector(Vertex);		
	}
}

/** 
 * Move vertex to new location and update vertex hash
 * @param VertId      - index of moved vertex
 * @param inV         - new location
 * @param bWorldSpace - if true, new location is in world space
 */
void UNavigationMeshBase::MoveVert( VERTID VertId, const FVector& inV, UBOOL bWorldSpace)
{
	FMeshVertex& Vertex = Verts(VertId);
	VertHash->RemoveSinglePair(Vertex, VertId);

	Vertex = (bWorldSpace) ? FVector(W2LTransformFVector(inV)) : inV;

	VertHash->Add(Vertex, VertId);
}

UBOOL FMeshVertex::operator==( const FMeshVertex& V ) const
{
	return VertexHash == V.VertexHash;
}

/**
 *	will attempt to determine if this vertex is on the boundary of the mesh
 * @param MyID - the ID of the vertex to test
 * @return - TRUE if this vert is on the border (some false positives, no false negatives)
 */
UBOOL FMeshVertex::IsBorderVert(VERTID MyID)
{
	FLOAT TotalAngle = 0.f;

	for(INT ContainingIdx=0;ContainingIdx < ContainingPolys.Num();ContainingIdx++)
	{
		FNavMeshPolyBase* CurPoly = ContainingPolys(ContainingIdx);
	
		// ** find the angle of the tri that uses this vert

		INT LocalIdx = -1;
		CurPoly->PolyVerts.FindItem(MyID,LocalIdx);
		checkSlowish(LocalIdx >=0);
		if(LocalIdx<0)
		{
			continue;
		}

		FVector Next = CurPoly->GetVertLocation( (LocalIdx+1) % CurPoly->PolyVerts.Num(), LOCAL_SPACE );
		FVector Prev = CurPoly->GetVertLocation( (LocalIdx-1 >= 0) ? LocalIdx-1 : CurPoly->PolyVerts.Num()-1, LOCAL_SPACE  );
		
		FLOAT ThisAngle = appAcos((Next - *this).SafeNormal() | (Prev - *this).SafeNormal());
		TotalAngle += ThisAngle;
	}

	return !appIsNearlyEqual(TotalAngle,(FLOAT)(2.0f*PI),0.01f);
}

FNavMeshPolyBase* FMeshVertex::GetContainingPolyAtIdx(INT Idx, UNavigationMeshBase* NavMesh )
{
	return (ContainingPolys.Num() > 0 ) ? ContainingPolys(Idx) : &NavMesh->Polys(PolyIndices(Idx));
}

/** Sets and returns OwningPylon */
APylon* UNavigationMeshBase::GetPylon()
{
	if( OwningPylon != NULL )
	{
		return OwningPylon;
	}

	OwningPylon = Cast<APylon>(GetOuter());
	return OwningPylon;
}

/**
 *	searches the passed obstacle poly for geometry that should be linked to the passed cross pylon edge'
 * (called from LinkToObstacleGeo)
 * @param ObstacleMesh - the mesh to search
 * @param Poly - the obstacle poly to link the edge to
 * @param CPEdge - cross pylon edge we're trying to link this poly to
 * @param EdgeIdx - index of CPEdge
 * @param bDynamicEdge - TRUE if this edge is dynamic (added at runtime)
 * @param bTestOnly - only return a status, don't modify anything
 * @return TRUE on success
 */
FLOAT ExpansionObstacleEdgeVertTolerance = 15.0f;
#define OBSTACLE_EDGE_VERT_TOLERANCE ExpansionObstacleEdgeVertTolerance
UBOOL TryToLinkPolyToEdge(UNavigationMeshBase* ObstacleMesh, FNavMeshPolyBase& Poly, const FVector& EdgeVert0, const FVector& EdgeVert1, WORD EdgeIdx, UBOOL bDynamicEdge, FNavMeshCrossPylonEdge* CPEdge=NULL, UBOOL bTestOnly=FALSE)
{
	FVector BottomVert0 = FVector(0.f);
	FVector BottomVert1 = FVector(0.f);		

	// find the bottom edge
	FVector BestCtr = FVector(0.f,0.f,BIG_NUMBER);
	for(INT VertIdx=0;VertIdx<Poly.PolyVerts.Num();++VertIdx)
	{
		INT NextIdx = (VertIdx+1) % Poly.PolyVerts.Num();
		FVector EdgeVert0 = ObstacleMesh->GetVertLocation(Poly.PolyVerts(VertIdx),LOCAL_SPACE);
		FVector EdgeVert1 = ObstacleMesh->GetVertLocation(Poly.PolyVerts(NextIdx),LOCAL_SPACE);

		FVector CurEdgeCtr = (EdgeVert0 + EdgeVert1)*0.5f;
		if((EdgeVert0-EdgeVert1).Size2D() > 0.1f && CurEdgeCtr.Z < BestCtr.Z)
		{
			BestCtr=CurEdgeCtr;
			BottomVert0=EdgeVert0;
			BottomVert1=EdgeVert1;
		}
	}

	// if the center of this edge is within the poly's bottom edge, add ourselves to the poly's edge list
	// NOTE: this is adding the ID of this edge, to a poly in another graph!
	FVector EdgeCtrNoZ = (EdgeVert0+EdgeVert1)*0.5f;

	FVector OldBottomVert0 = BottomVert0;
	FVector OldBottomVert1 = BottomVert1;

	EdgeCtrNoZ.Z = BottomVert0.Z = BottomVert1.Z = 0.f;
	if((BottomVert0-BottomVert1).IsNearlyZero(0.1f))
	{
		debugf(TEXT("0:%s - 1:%s Dist: %.2f ---"), *BottomVert0.ToString(), *BottomVert1.ToString(), (BottomVert0-BottomVert1).Size());
		for(INT Idx=0;Idx<Poly.PolyVerts.Num();++Idx)
		{
			debugf(TEXT("PolyVert[%i](%i): %s"),Idx,Poly.PolyVerts(Idx),*ObstacleMesh->GetVertLocation(Poly.PolyVerts(Idx)).ToString());
		}
	}
	//checkSlowish(!(BottomVert0-BottomVert1).IsNearlyZero(0.1f));

	UBOOL bValid = FALSE;
	FVector Closest=FVector(0.f);
	FLOAT Dist = PointDistToSegment(EdgeCtrNoZ,BottomVert0,BottomVert1,Closest);
	if(Dist < OBSTACLE_EDGE_VERT_TOLERANCE)
	{
		bValid=TRUE;
	}
		
	if(bValid)
	{
		if( !bTestOnly )
		{
			Poly.PolyEdges.AddUniqueItem(EdgeIdx);
			if( CPEdge != NULL && CPEdge->ObstaclePolyID == MAXWORD )
			{
				CPEdge->ObstaclePolyID=Poly.Item;		
			}
		}
	}

	return bValid;
}

/**
 *	Finds obstacle geo near this edge and links the edge to it
 * @param EdgeIdx - index of new edge 
 * @param ObstacleMesh - obstacle mesh to link this edge to 
 * @param bDynamicEdge - TRUE if this edge is dynamic (added at runtime)
 */
void FNavMeshCrossPylonEdge::LinkToObstacleGeo(WORD EdgeIdx, UNavigationMeshBase* ObstacleMesh, UBOOL bDynamicEdge)
{
	check(EdgeIdx < MAXVERTID);

	if(ObstacleMesh->Polys.Num() > 0)
	{
		for(INT PolyIdx=0;PolyIdx<ObstacleMesh->Polys.Num();PolyIdx++)
		{
			FNavMeshPolyBase& Poly = ObstacleMesh->Polys(PolyIdx);
			TryToLinkPolyToEdge(ObstacleMesh,Poly,GetVertLocation(0,LOCAL_SPACE),GetVertLocation(1,LOCAL_SPACE),EdgeIdx,bDynamicEdge,this);
		}
	}
	else
	{
		FNavMeshPolyBase* CurPoly = NULL;

		for( PolyList::TIterator Itt = ObstacleMesh->BuildPolys.GetHead();Itt;++Itt)
		{
			CurPoly = *Itt;
			TryToLinkPolyToEdge(ObstacleMesh,*CurPoly,GetVertLocation(0,LOCAL_SPACE),GetVertLocation(1,LOCAL_SPACE),EdgeIdx,bDynamicEdge, this);
		}
	}
}

INT UNavigationMeshBase::GetResourceSize()
{
	if (GExclusiveResourceSizeMode)
	{
		return 0;
	}
	else
	{
		FArchiveCountMem CountBytesSize(this);
		INT ResourceSize = CountBytesSize.GetNum();
		return ResourceSize;
	}
}

/**
 *	adds an edge at the passed locations linking the connectingpolys together (to be used for edges that link between meshes)
 *  @param inV1 - first vert of edge
 *  @param inV2 - second vert of edge
 *  @param ConnectedPolys - polys this edge connects
 *  @param SupportedEdgeWidth - the width of the guy that this edge supports
 *  @param EdgeGroupID - the ID of the edge group which this is a part of
 */
void UNavigationMeshBase::AddCrossPylonEdge( const FVector& inV1, const FVector& inV2, TArray<FNavMeshPolyBase*>& ConnectedPolys, FLOAT SupportedEdgeWidth, BYTE EdgeGroupID )
{
	FNavMeshPolyBase* Poly0 = ConnectedPolys(0);
	FNavMeshPolyBase* Poly1 = ConnectedPolys(1);

	APylon* Pylon0 = Poly0->NavMesh->GetPylon();
	APylon* Pylon1 = Poly1->NavMesh->GetPylon();

	VERTID Poly0Vert0Idx = Poly0->NavMesh->AddVert(inV1,WORLD_SPACE,-1.f,TRUE);
	VERTID Poly0Vert1Idx = Poly0->NavMesh->AddVert(inV2,WORLD_SPACE,-1.f,TRUE);

	VERTID Poly1Vert0Idx = Poly1->NavMesh->AddVert(inV1,WORLD_SPACE,-1.f,TRUE);
	VERTID Poly1Vert1Idx = Poly1->NavMesh->AddVert(inV2,WORLD_SPACE,-1.f,TRUE);

	// check for vert index overflow
	if( Poly0Vert0Idx == MAXVERTID || Poly0Vert1Idx == MAXVERTID ||
		Poly1Vert0Idx == MAXVERTID || Poly1Vert1Idx == MAXVERTID )
	{
		return;
	}

	// ** add edge to pylon0
	WORD NewEdgeID = MAXWORD;
	UNavigationMeshBase* Mesh = Poly0->NavMesh;
	// MT->Have to iterate through all edges here since crosspylonptr cache is not (and can not be) set up yet
	//		Loop through all edges...
	//		If any of the edges share the same verts (in either order) just use the existing edge
	FNavMeshCrossPylonEdge* Edge = NULL;
	for( INT EdgeIdx = 0; EdgeIdx < Mesh->GetNumEdges(); EdgeIdx++ )
	{
		FNavMeshEdgeBase* BaseEdge = Mesh->GetEdgeAtIdx(EdgeIdx);
		if(BaseEdge->IsCrossPylon())
		{
			Edge = (FNavMeshCrossPylonEdge*)BaseEdge;
			if( (((*Edge->Poly0Ref) == Poly0 && (*Edge->Poly1Ref) == Poly1) ||
				((*Edge->Poly0Ref) == Poly1 && (*Edge->Poly1Ref) == Poly0)) &&
				EdgesAreEqual(Edge->GetVertLocation(0),Edge->GetVertLocation(1),inV1,inV2)
				)
			{
				NewEdgeID = EdgeIdx;

				// ensure this poly knows to ref this edge
				Poly0->PolyEdges.AddUniqueItem(NewEdgeID);
				break;
			}
		}
	}

	// Otherwise...
	if( NewEdgeID == MAXWORD )
	{
		// Add the edge to the pool
		FNavMeshCrossPylonEdge TempEdge = FNavMeshCrossPylonEdge(Mesh, 
																Pylon0,Poly0->Item,Poly0Vert0Idx,Poly0Vert1Idx,
			Pylon1,Poly1->Item,Poly1Vert0Idx,Poly1Vert1Idx);
		FNavMeshCrossPylonEdge* NewEdge = Pylon0->NavMeshPtr->AddEdgeData<FNavMeshCrossPylonEdge>(TempEdge,NewEdgeID);
		Poly0->PolyEdges.AddUniqueItem(NewEdgeID);

#if !PS3
		checkSlowish(Poly0->Item < Poly0->NavMesh->Polys.Num());
		checkSlowish(Poly1->Item < Poly1->NavMesh->Polys.Num());
#endif

		NewEdge->SetPoly0( Poly0 );
		NewEdge->SetPoly1( Poly1 );
		NewEdge->UpdateEdgePerpDir();
		Edge = NewEdge;

		// carry over supported edge width
		NewEdge->EffectiveEdgeLength = SupportedEdgeWidth;
		// carry over edge Group ID
		NewEdge->EdgeGroupID = EdgeGroupID;
	}

	// link obstacle geo (if any) to this edge
	UNavigationMeshBase* ObstacleMesh = Pylon0->ObstacleMesh;
	if(ObstacleMesh != NULL)
	{
		Edge->LinkToObstacleGeo(NewEdgeID,ObstacleMesh);
	}





	// ** add edge to pylon1
	NewEdgeID = MAXWORD;
	Mesh = Poly1->NavMesh;
	// MT->Have to iterate through all edges here since crosspylonptr cache is not (and can not be) set up yet
	//		Loop through all edges...
	//		If any of the edges share the same verts (in either order) just use the existing edge
	for( INT EdgeIdx = 0; EdgeIdx < Mesh->GetNumEdges(); EdgeIdx++ )
	{
		FNavMeshEdgeBase* BaseEdge = Mesh->GetEdgeAtIdx(EdgeIdx);
		if(BaseEdge->IsCrossPylon())
		{
			Edge = (FNavMeshCrossPylonEdge*)BaseEdge;

			if( ((*Edge->Poly0Ref) == Poly0 && (*Edge->Poly1Ref) == Poly1) ||
				((*Edge->Poly0Ref) == Poly1 && (*Edge->Poly1Ref) == Poly0)
				)
			{
				NewEdgeID = EdgeIdx;
				// ensure this poly knows to ref this edge
				Poly1->PolyEdges.AddUniqueItem(NewEdgeID);

				break;
			}
		}
	}

	// Otherwise...
	if( NewEdgeID == MAXWORD )
	{
		// Add the edge to the pool
		FNavMeshCrossPylonEdge TempEdge = 	FNavMeshCrossPylonEdge(Mesh,
																	Pylon1,Poly1->Item,Poly1Vert0Idx,Poly1Vert1Idx,
			Pylon0,Poly0->Item,Poly0Vert0Idx,Poly0Vert1Idx);

		FNavMeshCrossPylonEdge* NewEdge = Pylon1->NavMeshPtr->AddEdgeData<FNavMeshCrossPylonEdge>(TempEdge,NewEdgeID);
		Poly1->PolyEdges.AddUniqueItem(NewEdgeID);
		NewEdge->SetPoly0( Poly1 );
		NewEdge->SetPoly1( Poly0 );
		NewEdge->UpdateEdgePerpDir();

		Edge=NewEdge;

		// carry over supported edge width
		NewEdge->EffectiveEdgeLength = SupportedEdgeWidth;
		// carry over edge Group ID
		NewEdge->EdgeGroupID = EdgeGroupID;

	}

	// link obstacle geo (if any) to this edge
	ObstacleMesh = Pylon1->ObstacleMesh;
	if(ObstacleMesh != NULL)
	{
		Edge->LinkToObstacleGeo(NewEdgeID,ObstacleMesh);
	}

}

/**
 *	will add segments marking off 'border' portions of this poly.. e.g. sections of the boundary of the poly which are not adjacent to other polys
 * @param Poly - the poly to add border segments for
 * @param BorderEdgeSegments - out array stuffed with border edge segment structs
 */
void AddBorderEdgeSegmentsForPoly(FNavMeshPolyBase* Poly, TArray<UNavigationMeshBase::BorderEdgeInfo>& BorderEdgeSegments)
{
	// if we have a submesh, add its border segments in lieu of ours
	if(Poly->NumObstaclesAffectingThisPoly>0)
	{
		// ensure poly structure is up to date
		checkSlowish(!Poly->GetObstacleInfo()->bNeedRecompute);

		FPolyObstacleInfo* Info = Poly->GetObstacleInfo();		
		checkSlowish(Info != NULL && Info->SubMesh != NULL);
		
		// for each poly, add border edge segs 
		for(INT SubPolyIdx=0;SubPolyIdx<Info->SubMesh->Polys.Num();++SubPolyIdx)
		{
			FNavMeshPolyBase* CurPoly = &Info->SubMesh->Polys(SubPolyIdx);
			AddBorderEdgeSegmentsForPoly(CurPoly,BorderEdgeSegments);
		}		

		return;
	}

	// use borderedgesegmentlist on the navmesh to figure out which of this poly's edges are border segs
	for (INT Idx=0;Idx<Poly->NavMesh->BorderEdgeSegments.Num();++Idx)
	{
		UNavigationMeshBase::BorderEdgeInfo& EdgeInfo = Poly->NavMesh->BorderEdgeSegments(Idx);
		if( EdgeInfo.Poly == Poly->Item )
		{
			BorderEdgeSegments.AddItem(EdgeInfo);
		}
	}
}

/**
* Mark an edge as being used by the passed NavigationHandle
* @param Edge - the edge which needs to be marked
* @param Handle - the handle using the edge
*/
UBOOL bDoActiveClaim=TRUE;
void UNavigationMeshBase::MarkEdgeAsActive(FNavMeshEdgeBase* Edge, UNavigationHandle* Handle)
{
	IInterface_NavigationHandle* ActiveInterface = InterfaceCast<IInterface_NavigationHandle>(Handle->GetOuter());
	if( bDoActiveClaim && ActiveInterface != NULL )
	{
		static TArray<FNavMeshEdgeBase*> EdgesInGroup;
		EdgesInGroup.Reset();

		Edge->GetAllEdgesInGroup(Edge->GetPoly0(),EdgesInGroup);

		for(INT EdgeIdx=0;EdgeIdx<EdgesInGroup.Num();++EdgeIdx)
		{
			FNavMeshEdgeBase* GroupEdge = EdgesInGroup(EdgeIdx);
			GroupEdge->ExtraEdgeCost += ActiveInterface->ExtraEdgeCostToAddWhenActive(GroupEdge);
		}
		//          debugf(TEXT("%s MarkActiveEdge %d/%d"), *Handle->GetOuter()->GetName(), ActiveInterface->ExtraEdgeCostToAddWhenActive(), Edge->ExtraEdgeCost );
	}
}


/**
* Removes a handle from an edge's list of actively using handles
* @param Edge - the edge which needs to be un-marked
* @param Handle - the handle using the edge
*/
void UNavigationMeshBase::UnMarkEdgeAsActive(FNavMeshEdgeBase* Edge, UNavigationHandle* Handle)
{
	IInterface_NavigationHandle* ActiveInterface = InterfaceCast<IInterface_NavigationHandle>(Handle->GetOuter());
	if(bDoActiveClaim &&  ActiveInterface != NULL )
	{
		static TArray<FNavMeshEdgeBase*> EdgesInGroup;
		EdgesInGroup.Reset();

		Edge->GetAllEdgesInGroup(Edge->GetPoly0(),EdgesInGroup);

		for(INT EdgeIdx=0;EdgeIdx<EdgesInGroup.Num();++EdgeIdx)
		{
			FNavMeshEdgeBase* GroupEdge = EdgesInGroup(EdgeIdx);
			GroupEdge->ExtraEdgeCost = Max<INT>( GroupEdge->ExtraEdgeCost - ActiveInterface->ExtraEdgeCostToAddWhenActive(GroupEdge), 0 );
		}		

//		debugf(TEXT("%s UnMarkActiveEdge %d/%d"), *Handle->GetOuter()->GetName(), ActiveInterface->ExtraEdgeCostToAddWhenActive(), Edge->ExtraEdgeCost );
	}
}

/** 
 * adds the cross pylon edge that references this mesh to a list so we know when to clean it up
 * @param EdgeThatRefs - the edge that is reffing this mesh we need to track
 */
void UNavigationMeshBase::NotifyEdgeRefOfMesh(FNavMeshCrossPylonEdge* EdgeThatRefs)
{
	IncomingDynamicEdges.AddHead(EdgeThatRefs);
}

/**
 * removes an entry in the reffing edge list (e.g. an edge in another mesh that used to be reffing us is being destroyed, so stop tracking it)
 * @param EdgeThatUsedtoRef - the edge that used to ref this mesh that no longer does
 */
void UNavigationMeshBase::RemoveEdgeRefOfMesh(FNavMeshCrossPylonEdge* EdgeThatNoLongerRefs)
{
	IncomingDynamicEdges.RemoveNode(EdgeThatNoLongerRefs);
}

#if !FINAL_RELEASE 
UBOOL UNavigationMeshBase::AreNavMeshConnectionsValid (void)
{
	return (AreEdgesValid() && ArePolysValid() && AreVerticesValid());
}

UBOOL UNavigationMeshBase::AreEdgesValid(void)
{
	for (INT scan = 0; scan < EdgePtrs.Num(); ++scan)
	{
		FNavMeshEdgeBase* edge = EdgePtrs(scan);
		if (edge->Vert0 >= Verts.Num())
		{
			return FALSE;
		}

		if (edge->Vert1 >= Verts.Num())
		{
			return FALSE;
		}
	}
	
	return TRUE;
}

UBOOL UNavigationMeshBase::ArePolysValid(void)
{
	for (INT scan = 0; scan < Polys.Num(); ++scan)
	{
		FNavMeshPolyBase& Poly = Polys(scan);
		for (INT VertScan = 0; VertScan < Poly.PolyVerts.Num(); ++VertScan)
		{
			VERTID VertIndex = Poly.PolyVerts(VertScan);
			if (VertIndex >= Verts.Num())
			{
				return FALSE;
			}
		}

		//Obstacle mesh actually references edges from nav mesh
		if (GetPylon()->NavMeshPtr->EdgePtrs.Num() <= 0)
		{
			GetPylon()->NavMeshPtr->PopulateEdgePtrCache();
		}
		TArray<FNavMeshEdgeBase*> EdgesToTest = GetPylon()->NavMeshPtr->EdgePtrs;

		
		for (INT EdgeScan = 0; EdgeScan < Poly.PolyEdges.Num(); ++EdgeScan) 
		{
			WORD EdgeIndex = Poly.PolyEdges(EdgeScan);
			if (EdgeIndex >= EdgesToTest.Num())
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

UBOOL UNavigationMeshBase::AreVerticesValid(void)
{
	for(INT Scan = 0; Scan < Verts.Num(); ++Scan)
	{
		FMeshVertex& Vert = Verts(Scan);
		for(INT PolyScan = 0; PolyScan < Vert.PolyIndices.Num(); ++PolyScan)
		{
			WORD PolyIndex = Vert.PolyIndices(PolyScan);
			if (PolyIndex >= Polys.Num())
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}
#endif

/**
* builds a list of poly edges which are on the border of the mesh, for use with stitching etc..
*/
void UNavigationMeshBase::BuildBorderEdgeList(UBOOL bForSubMesh)
{
// 	if(bForSubMesh)
// 	{
// 		// for each 
// 	}

	if( GetPylon()->bImportedMesh )
	{
		// For imported meshes, treat all edges as on the border
		BorderEdgeSegments.Reset();
		for(INT PolyIdx=0;PolyIdx<Polys.Num();PolyIdx++)
		{
			FNavMeshPolyBase* Poly = &Polys(PolyIdx);
			for(INT PolyVertIdx=0;PolyVertIdx<Poly->PolyVerts.Num();PolyVertIdx++)
			{
				VERTID CurID = Poly->PolyVerts(PolyVertIdx);
				VERTID NextID = Poly->PolyVerts((PolyVertIdx+1) % Poly->PolyVerts.Num() );
				BorderEdgeSegments.AddItem(UNavigationMeshBase::BorderEdgeInfo(CurID,NextID,Poly->Item));
			}
		}
	}
	// MT-> border edge segments for normal meshes get created during obstacle mesh generation
}

/**
 * Perform simple adjustment to neighbour NavMeshes after pylon stops moving, just before recreating dynamic edges.
 * It's neccesary for Recast generated meshes, since they aren't aligned to edges of collision geometry,
 * but to voxel grid. This small offset prevents creation of dynamic edges.
 *
 * UE generated NavMesh won't be modified and it may cause problems when it's used in DynamicPylon connecting to Recast generated mesh
 * (verified in CheckForErrors)
 */
void ADynamicPylon::ApplyDynamicSnap()
{
#if WITH_RECAST
	if (GEngine->bUseRecastNavMesh)
	{
		if (NavMeshPtr == NULL || ObstacleMesh == NULL || NavMeshGenerator != NavMeshGenerator_Recast)
			return;

		// make sure that VertHash is created
		NavMeshPtr->PrepareVertexHash();

		// try to snap verts
		TArray<UNavigationMeshBase::FDynamicSnapInfo> SnapData;
		for (INT i = 0; i < NavMeshPtr->Verts.Num(); i++)
		{
			FMeshVertex& SnapVert = NavMeshPtr->Verts(i);
			if (SnapVert.PolyIndices.Num() <= 0 || !SnapVert.IsBorderVert(i))
				continue;

			FVector SnapVertLoc = NavMeshPtr->L2WTransformFVector(SnapVert);
			TArray<FNavMeshPolyBase*> Polys;
			UNavigationHandle::GetAllPolysFromPos(SnapVertLoc, RecastSnapExtent, Polys, TRUE);

			FVector BestMatch;
			FLOAT BestScore = -1.0;

			for (INT iPoly = 0; iPoly < Polys.Num(); iPoly++)
			{
				FNavMeshPolyBase* PolyItem = Polys(iPoly);
				APylon* PolyPylon = PolyItem->NavMesh->GetPylon();

				if (PolyItem->NavMesh != NavMeshPtr)
				{
					for (INT iVert = 0; iVert < PolyItem->PolyVerts.Num(); iVert++)
					{
						FVector PolyVert = PolyItem->GetVertLocation(iVert, WORLD_SPACE);
						FVector Diff = PolyVert - SnapVertLoc;

						if (Abs(Diff.X) <= RecastSnapExtent.X && 
							Abs(Diff.Y) <= RecastSnapExtent.Y &&
							Abs(Diff.Z) <= RecastSnapExtent.Z &&
							!IsSnapBlocked(PolyVert, SnapVertLoc))
						{
							FLOAT VertScore = Diff.SizeSquared();
							if (BestScore < 0 || BestScore > VertScore)
							{
								BestScore = VertScore;
								BestMatch = PolyVert;
							}
						}
					}
				}
			}

			if (BestScore > 0)
			{
				// skip snap when navmesh already has a vertex on desired location
				VERTID ExistingVertId = NavMeshPtr->FindVert(BestMatch, WORLD_SPACE);
				if (ExistingVertId == 65535)
				{
					UNavigationMeshBase::FDynamicSnapInfo NewSnapData;
					NewSnapData.VertId = i;
					NewSnapData.VertWorldLocation = BestMatch;
					SnapData.AddItem(NewSnapData);
				}
			}
		}

		NavMeshPtr->ApplyDynamicSnap(SnapData, RecastBoundsZOffset);
	}
#endif
}

void ADynamicPylon::FlushDynamicEdges()
{
	Super::FlushDynamicEdges();

	PathList.Empty();
}

void APylon::FlushDynamicEdges()
{
	if(NavMeshPtr != NULL)
	{
		//Need to revert dynamic snaps before removeing dynamic edges
		//The dynamic snap can change the position of dynamic verts
		NavMeshPtr->RevertDynamicSnap();

		NavMeshPtr->FlushDynamicEdges();
	}
}

void ADynamicPylon::RebuildDynamicEdges()
{
	//debugf(TEXT("!! RebuildDynamicEdges on %s"), *GetName());
	ApplyDynamicSnap();

	if(NavMeshPtr != NULL)
	{
		NavMeshPtr->bSkipDynamicSnapRevert = TRUE;		// protect snap data from being instantly reverted by dynamic edge rebuild

		NavMeshPtr->RebuildDynamicEdgeConnections();

		NavMeshPtr->bSkipDynamicSnapRevert = FALSE;		// disable snap protection
	}
}

void ADynamicPylon::PylonMoved()
{
	Super::PylonMoved();
	if(!bMoving)
	{
		eventStartedMoving();
	}
	else
	{
		SetTimer(0.5f,FALSE,TEXT("StoppedMoving"));
	}
}

void ADynamicPylon::PreBeginPlay()
{
	if(NavMeshPtr != NULL)
	{
		NavMeshPtr->InitTransform(this);
	}
	if( ObstacleMesh != NULL )
	{
		ObstacleMesh->InitTransform(this);
	}

	Super::PreBeginPlay();

	// ensure we are in the cross level actor list
	ULevel* const Level = GetLevel();
	Level->CrossLevelActors.AddItem( this );
	bHasCrossLevelPaths = TRUE;

	// update octree position 
	RemoveFromNavigationOctree();
	AddToNavigationOctree();

}

void ADynamicPylon::FindBase()
{
	if ( GWorld->HasBegunPlay() )
	{
		return;
	}

	SetZone(1,1);
	if( ShouldBeBased() )
	{
		// not using find base, because don't want to fail if LD has navigationpoint slightly interpenetrating floor
		FCheckResult Hit(1.f);
		AScout *Scout = FPathBuilder::GetScout();
		check(Scout != NULL && "Failed to find scout for point placement");
		// get the dimensions for the average human player
		FVector HumanSize = Scout->GetSize(FName(TEXT("Human"),FNAME_Find));
		FVector CollisionSlice(HumanSize.X, HumanSize.X, 1.f);
		// and use this node's smaller collision radius if possible
		if (CylinderComponent->CollisionRadius < HumanSize.X)
		{
			CollisionSlice.X = CollisionSlice.Y = CylinderComponent->CollisionRadius;
		}
		// check for placement
#if WITH_EDITOR 

		FMemMark Mark(GMainThreadMemStack);
		FCheckResult* FirstHit = GWorld->MultiLineCheck
			(
			GMainThreadMemStack, 
			Location - FVector(0,0, 4.f * CylinderComponent->CollisionHeight),
			Location,
			CollisionSlice,
			TRACE_AllBlocking,
			Scout
			);

		//
		TArray<AActor*> ActorsWeMessedWith;
		for( FCheckResult* Check = FirstHit; Check!=NULL; Check=Check->GetNext() )
		{
			if (Check->Actor != NULL && !Check->Actor->bPathTemp)
			{
				ActorsWeMessedWith.AddItem(Check->Actor);
				Check->Actor->SetCollisionForPathBuilding(TRUE);
			}
		}
		Mark.Pop();

		// now do another MLC to see what's left
		FMemMark Mark2(GMainThreadMemStack);
		FirstHit = GWorld->MultiLineCheck
			(
			GMainThreadMemStack, 
			Location - FVector(0,0, 4.f * CylinderComponent->CollisionHeight),
			Location,
			CollisionSlice,
			TRACE_AllBlocking,
			Scout
			);

		//
		AActor* BestBase = (FirstHit != NULL) ? FirstHit->Actor : NULL;
		for( FCheckResult* Check = FirstHit; Check!=NULL; Check=Check->GetNext() )
		{
			if (Check->Actor != NULL )
			{
				if( BestBase != NULL )
				{
					if( !Check->Actor->IsStatic())
					{
						BestBase = Check->Actor;
					}
				}
				else
				{
					BestBase = Check->Actor;
				}

				if( !BestBase->IsStatic() )
				{
					Hit = *Check;
					break;
				}
			}
		}
		Mark2.Pop();

		if (Hit.Actor != NULL)
		{
			if (Hit.Normal.Z > Scout->WalkableFloorZ)
			{
				GWorld->FarMoveActor(this, Hit.Location + FVector(0.f,0.f,CylinderComponent->CollisionHeight-2.f),0,1,0);
			}
			else
			{
				Hit.Actor = NULL;
			}
		}

#endif

#if WITH_EDITOR
		for(INT ActorIdx=0;ActorIdx<ActorsWeMessedWith.Num();++ActorIdx)
		{
			ActorsWeMessedWith(ActorIdx)->SetCollisionForPathBuilding(FALSE);
		}
#endif


		SetBase(Hit.Actor, Hit.Normal);
		if (GoodSprite != NULL)
		{
			GoodSprite->HiddenEditor = FALSE;
		}
		if (BadSprite != NULL)
		{
			BadSprite->HiddenEditor = TRUE;
		}

	}
}

#if WITH_EDITOR
/** 
 * Overriden for verifing generator type
 */
void ADynamicPylon::CheckForErrors()
{
	if (NavMeshGenerator != NavMeshGenerator_Recast)
	{
		UBOOL bHasRecastPylons = FALSE;
		for (FActorIterator It; It; ++It)
		{
			APylon* iPylon = Cast<APylon>(*It);
			if (iPylon != NULL && iPylon->NavMeshGenerator == NavMeshGenerator_Recast)
			{
				bHasRecastPylons = TRUE;
				break;
			}
		}

		if (bHasRecastPylons)
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString( LocalizeUnrealEd( "MapCheck_Message_DynamicPylonGenerator" ) ), TEXT( "PylonGeneratorWarn" ) );
		}
	}

	Super::CheckForErrors();
}
#endif

// returns TRUE if the path from Poly back to start has an edge which is linked to a switch which is linked to this 
// pylon
UBOOL AAISwitchablePylon::HasSwitchLinkedToMeInPath(FNavMeshEdgeBase* Edge, FNavMeshPolyBase* Poly)
{
	FNavMeshEdgeBase* CurEdge = Edge;
	while(CurEdge != NULL)
	{
		// if we're already inside the pylon, go with it
		if(CurEdge->NavMesh->GetPylon() == this)
		{
			return TRUE;
		}

		if(CurEdge->GetEdgeType() == NAVEDGE_PathObject)
		{
			FNavMeshPathObjectEdge* POEdge = static_cast<FNavMeshPathObjectEdge*>(CurEdge);
			IInterface_NavMeshPathSwitch* SwitchInt = InterfaceCast<IInterface_NavMeshPathSwitch>(*POEdge->PathObject);
			if(SwitchInt != NULL && SwitchInt->IsLinkedTo(this))
			{
				return TRUE;
			}
		}

		CurEdge = CurEdge->PreviousPathEdge;
	}
	return FALSE;
}

// overidden to deny access to edges when we're disabled and the path doesn't incorporate a switch linked to this pylon
UBOOL AAISwitchablePylon::CostFor(const FNavMeshPathParams& PathParams,
								  const FVector& PreviousPoint,
								  FVector& out_PathEdgePoint,
								  FNavMeshEdgeBase* Edge,
								  FNavMeshPolyBase* SourcePoly,
								  INT& out_Cost)
{
	// if the gate is open, don't do anything
	if( bOpen )
	{
		return FALSE;
	}

	// otherwise unless the path has a switch linked to this pylon in it, block it
	if( !HasSwitchLinkedToMeInPath(Edge,SourcePoly) )
	{
		out_Cost = UCONST_BLOCKEDPATHCOST;
	}

	return TRUE;
}

/**
 * this is for getting meshes to re-link themselves to the rest of the world after being moved at runtime
 * this will wipe all dynamic edges, and create new ones where applicable
 */
void UNavigationMeshBase::RebuildDynamicEdgeConnections()
{
	APylon* MyPylon = GetPylon();

	check(MyPylon);
	MyPylon->FlushDynamicEdges();

	TArray<APylon*> Pylons;
	FBox PylonBounds = MyPylon->GetBounds(WORLD_SPACE).ExpandBy(10.0f);

	UNavigationHandle::GetIntersectingPylons(PylonBounds.GetCenter(), PylonBounds.GetExtent(),Pylons);
	// for each intersecting pylon, add links from MyPlon to it, and from it to MyPylon
	for(INT IsectPylonIdx=0;IsectPylonIdx<Pylons.Num();IsectPylonIdx++)
	{

		APylon* CurPylon = Pylons(IsectPylonIdx);

		if(CurPylon == MyPylon || CurPylon->NavMeshPtr == NULL)
		{
			continue;
		}

		CreateDynamicEdgesForPylonAtoPylonB(MyPylon,CurPylon);
		if( MyPylon->GetReachSpecTo(CurPylon) == NULL )
		{
			CreateDynamicEdgesForPylonAtoPylonB(CurPylon,MyPylon);
		}
	}
}

void UNavigationMeshBase::CreateDynamicEdgesForPylonAtoPylonB(APylon* PylonA, APylon* PylonB)
{
	//debugf(TEXT("!! CreateDynamicEdgesForPylonAtoPylonB A:%s B%s"),*PylonA->GetName(), *PylonB->GetName());

	#define CLOSE_EDGE_BUFFER 3.0f

	FBox PylonABounds = PylonA->GetBounds(WORLD_SPACE);
	FBox PylonBBounds = PylonB->GetBounds(WORLD_SPACE);
	static FLOAT BoundsExpansionBuffer = 10.0f;
	// expand a bit so we overlap on pylons we are right next to
	PylonABounds = PylonABounds.ExpandBy(BoundsExpansionBuffer); 
	PylonBBounds = PylonBBounds.ExpandBy(BoundsExpansionBuffer);

	

	// get a list of PylonB's polys which intersect PylonA's mesh
	TArray<FNavMeshPolyBase*> PylonBIntersectingPolys;
	PylonB->GetIntersectingPolys(PylonABounds.GetCenter(), PylonABounds.GetExtent(),PylonBIntersectingPolys,FALSE,TRUE);


	// if PylonA doesn't have a cached BorderEdgeSegment list then build our own list
	TArray<BorderEdgeInfo>& LocalBorderEdgeSegList=PylonA->NavMeshPtr->BorderEdgeSegments;
	TArray<BorderEdgeInfo> BuildInPlaceBorderEdgeSegList;
	//if(LocalBorderEdgeSegList.Num() < 1)
	{		// get a list of PylonA's polys which intersect PylonB's mesh
		TArray<FNavMeshPolyBase*> PylonAIntersectingPolys;
		PylonA->GetIntersectingPolys(PylonBBounds.GetCenter(), PylonBBounds.GetExtent(),PylonAIntersectingPolys,FALSE,TRUE);

		for(INT IsectIdx=0;IsectIdx<PylonAIntersectingPolys.Num();IsectIdx++)
		{
			FNavMeshPolyBase* CurPoly = PylonAIntersectingPolys(IsectIdx);
			//AddBorderEdgeSegmentsForPoly(CurPoly,BuildInPlaceBorderEdgeSegList);
			// if we have a submesh, add its border segments AND ours
			if(CurPoly->NumObstaclesAffectingThisPoly>0)
			{
				FPolyObstacleInfo* Info = CurPoly->GetObstacleInfo();		
				checkSlowish(Info != NULL && Info->SubMesh != NULL);

				// for each poly, add border edge segs 
				for(INT SubPolyIdx=0;SubPolyIdx<Info->SubMesh->Polys.Num();++SubPolyIdx)
				{
					FNavMeshPolyBase* CurSubPoly = &Info->SubMesh->Polys(SubPolyIdx);
					AddBorderEdgeSegmentsForPoly(CurSubPoly,BuildInPlaceBorderEdgeSegList);
				}		

			}
		}

		BuildInPlaceBorderEdgeSegList.Append(PylonA->NavMeshPtr->BorderEdgeSegments);
		LocalBorderEdgeSegList = BuildInPlaceBorderEdgeSegList;
	}


	static TArray<FNavMeshPolyBase*> PolysWithTempCollisionDisabled;
	PolysWithTempCollisionDisabled.Reset();
	// now then, we need to loop through all of our boundary segments and add edges for intersections with those boundary
	// edges and PylonB's mesh
	for(INT BorderEdgeSegIdx=0;BorderEdgeSegIdx<LocalBorderEdgeSegList.Num();BorderEdgeSegIdx++)
	{
		BorderEdgeInfo& Info = LocalBorderEdgeSegList(BorderEdgeSegIdx);
		FVector Pt0 = PylonA->NavMeshPtr->GetVertLocation(Info.Vert0,WORLD_SPACE);
		FVector Pt1 = PylonA->NavMeshPtr->GetVertLocation(Info.Vert1,WORLD_SPACE);
		FVector Ctr = (Pt0+Pt1)*0.5f;

		if(PylonBBounds.IsInside(Ctr) || PylonBBounds.IsInside(Pt0) || PylonBBounds.IsInside(Pt1))
		{
			// convert Pt0 and Pt1 into PyloNB's ref frame
			FVector CPLS_Pt0 = PylonB->NavMeshPtr->W2LTransformFVector(Pt0);
			FVector CPLS_Pt1 = PylonB->NavMeshPtr->W2LTransformFVector(Pt1);

			// for each poly in B that intersects A's bounds, see if there is an intersection between the current
			// boundary line of A and that poly
			for(INT PolyIdx=0;PolyIdx<PylonBIntersectingPolys.Num();PolyIdx++)
			{
				FNavMeshPolyBase* Poly = PylonBIntersectingPolys(PolyIdx);

				FVector Entry(0.f),Exit(0.f);

				const FVector CPLS_Delta = (CPLS_Pt1 - CPLS_Pt0);
				FVector Perp = ( CPLS_Delta ^ Poly->GetPolyNormal(LOCAL_SPACE) ).SafeNormal();
				if( ( Perp | (Poly->GetPolyCenter(LOCAL_SPACE) - CPLS_Pt0) ) < 0.f )
				{
					Perp *= -1.0f;
				}

				// move points toward poly center a bit so that edges which are very close to each other get picked up as overlap 
				FVector Buffered_CPLS_Pt0 = CPLS_Pt0 + Perp * CLOSE_EDGE_BUFFER;
				FVector Buffered_CPLS_Pt1 = CPLS_Pt1 + Perp * CLOSE_EDGE_BUFFER;

				// MT-NOTE-> how well does this work with slightly non planar?
				if(Poly->IntersectsPoly2D(Buffered_CPLS_Pt0,Buffered_CPLS_Pt1,Entry,Exit))
				{

					if((Entry-Exit).Size() > EDGE_VERT_TOLERANCE)
					{
						// grab entry/exit positions in world space
						FVector WS_Entry = PylonB->NavMeshPtr->L2WTransformFVector(Entry);
						FVector WS_Exit = PylonB->NavMeshPtr->L2WTransformFVector(Exit);

						FVector WS_Ctr = (WS_Entry + WS_Exit) * 0.5f;
	
						FVector ClosestPt(0.f);


						FVector ClosestPtOnPoly = WS_Ctr;
						Poly->AdjustPositionToDesiredHeightAbovePoly(ClosestPtOnPoly,0.f);
						if( Abs<FLOAT>(ClosestPtOnPoly.Z - WS_Ctr.Z) < NAVMESHGEN_MAX_STEP_HEIGHT )
						{
							// add edge to other mesh linking back to this one
							static TArray<FNavMeshPolyBase*> ConnectingPolys;
							ConnectingPolys.Reset(2);
							FNavMeshPolyBase* Poly0 = PylonA->NavMeshPtr->GetPolyFromId(Info.Poly);
							FNavMeshPolyBase* Poly1 = Poly;

							if( Poly0 == NULL || Poly1 == NULL )
							{
								continue;
							}

							ConnectingPolys.AddItem(Poly0);
							ConnectingPolys.AddItem(Poly1);
							
							// disable collision of obstacle mesh surfaces along boundary edges so we get accurate width generation
							for(INT Idx=0;Idx<ConnectingPolys.Num();++Idx)
							{
								FNavMeshPolyBase* CurPoly = ConnectingPolys(Idx);
								UNavigationMeshBase* ObsMesh = CurPoly->NavMesh->GetObstacleMesh();
								const FVector LS_Entry = ObsMesh->W2LTransformFVector(WS_Entry);
								const FVector LS_Exit = ObsMesh->W2LTransformFVector(WS_Exit);

								if(ObsMesh != NULL && ObsMesh->Polys.Num() > 0)
								{
									for(INT PolyIdx=0;PolyIdx<ObsMesh->Polys.Num();PolyIdx++)
									{
										FNavMeshPolyBase& Poly = ObsMesh->Polys(PolyIdx);
										if ( TryToLinkPolyToEdge(ObsMesh,Poly,LS_Entry,LS_Exit,MAXWORD,FALSE) )
										{
											PolysWithTempCollisionDisabled.AddItem(&Poly);
										}

									}	
								}
							}

							// add edge to both meshes
							static TArray<UNavigationMeshBase::FEdgeWidthPair> EdgePairs;
							EdgePairs.Reset();
							BuildEdgesFromSegmentSpan(WS_Entry,WS_Exit,EdgePairs,FALSE);

							static TArray<FNavMeshCrossPylonEdge*> CreatedEdges;
							CreatedEdges.Reset();

							for(INT EdgePairIdx=0;EdgePairIdx<EdgePairs.Num();++EdgePairIdx)
							{
								UNavigationMeshBase::FEdgeWidthPair& EdgePair = EdgePairs(EdgePairIdx);
								PylonB->NavMeshPtr->AddDynamicCrossPylonEdge<FNavMeshCrossPylonEdge>(EdgePair.Pt0,EdgePair.Pt1,ConnectingPolys,EdgePair.SupportedWidth,EdgePair.EdgeGroupID,FALSE,&CreatedEdges);	

								//debugf(TEXT("!! Added %i edge(s) between %s and %s! EdgePt0: %s EdgePt1: %s"), CreatedEdges.Num(),  *PylonA->GetName(), *PylonB->GetName(), *EdgePair.Pt0.ToString(), *EdgePair.Pt1.ToString());
								//debugf(TEXT("!! Poly0: %i (Obstacles: %i), Poly1: %i (Obstacles: %i)"), Poly0->Item, Poly0->NumObstaclesAffectingThisPoly, Poly1->Item, Poly1->NumObstaclesAffectingThisPoly);
								// maintain top level path network for early outs
								if( PylonB->GetReachSpecTo(PylonA) == NULL )
								{
									CreateSuperPathFromAToB(PylonB,PylonA);
								}

								if( PylonA->GetReachSpecTo(PylonB) == NULL )
								{
									CreateSuperPathFromAToB(PylonA,PylonB);
								}
							}


							// link new edges to obstacle polys (semi)permanently, make sure they're not in the 'to have collision re-enabled' list
							for(INT Idx=0;Idx<ConnectingPolys.Num();++Idx)
							{
								FNavMeshPolyBase* CurPoly = ConnectingPolys(Idx);
								UNavigationMeshBase* ObsMesh = CurPoly->NavMesh->GetObstacleMesh();
								if(ObsMesh != NULL && ObsMesh->Polys.Num() > 0)
								{
									for(INT PolyIdx=0;PolyIdx<ObsMesh->Polys.Num();PolyIdx++)
									{
										FNavMeshPolyBase& ObstaclePoly = ObsMesh->Polys(PolyIdx);

										for( INT EdgeIdx=0; EdgeIdx < CreatedEdges.Num(); ++EdgeIdx)
										{
											FNavMeshCrossPylonEdge* CPEdge = CreatedEdges(EdgeIdx);

											if( CPEdge->NavMesh == CurPoly->NavMesh )
											{
												if( TryToLinkPolyToEdge(ObsMesh,ObstaclePoly,CPEdge->GetVertLocation(0,LOCAL_SPACE),CPEdge->GetVertLocation(1,LOCAL_SPACE),MAXWORD-1,TRUE,CPEdge) )
												{
													PolysWithTempCollisionDisabled.RemoveItem(&ObstaclePoly);
												}
											}
										}
									}
								}
							}

						}
					}
				}
			}	

		}
	} // outer for loop


	// undo disabling of collision of obstacle mesh surfaces along boundary edges so we get accurate width generation
	for(INT PolyIdx=0;PolyIdx<PolysWithTempCollisionDisabled.Num(); ++PolyIdx)
	{
		FNavMeshPolyBase* Poly = PolysWithTempCollisionDisabled(PolyIdx);
		Poly->PolyEdges.RemoveItem(MAXWORD);
	}

}

void UNavigationMeshBase::FlushDynamicEdges()
{
	//debugf(TEXT("!! FlushDynamicEdges for %s"),*GetPylon()->GetName());
	if(GetPylon() != NULL && IsObstacleMesh())
	{
		DynamicEdges.Empty(DynamicEdges.Num());
	}
	else
	{
		// remove references to the edge first
		for(DynamicEdgeList::TIterator Itt(DynamicEdges);Itt;++Itt)
		{
			FNavMeshCrossPylonEdge* Edge = Itt.Value();		
			RemoveDynamicCrossPylonEdge(Edge);
		}

		if(GetPylon() != NULL && GetObstacleMesh() != NULL)
		{
			GetObstacleMesh()->DynamicEdges.Empty(DynamicEdges.Num());	
		}
		DynamicEdges.Empty(DynamicEdges.Num());
	}
}

/**
 * Called when our owning pylon gets OnRemoveFromWorld called on it. 
 * will flush all submeshes and clear refs to this mesh
 */
void UNavigationMeshBase::OnRemoveFromWorld()
{
	CleanupMeshReferences();
	for( PolyObstacleInfoList::TIterator It(PolyObstacleInfoMap);It;++It)
	{
		FPolyObstacleInfo& Info = It.Value();

		if(Info.SubMesh != NULL)
		{
			Info.SubMesh->CleanupMeshReferences(&Info);
		}

		if( Info.Poly != NULL )
		{
			Info.Poly->NumObstaclesAffectingThisPoly = 0;
		}
	}

	PolyObstacleInfoMap.Empty();
}


/**
 * called from RemoveDynamicCrossPylonEdge
 * - is responsible for clearing out verts added after the staticvertcount barrier periodically
 */
#define MAX_WASTE 100
#define MAX_VERT_THRESH MAXVERTID - MAX_WASTE

void UNavigationMeshBase::PruneDynamicVerts()
{

	// see how much waste we have
	const INT NumDynamicVerts = Verts.Num() - StaticVertCount;
	const INT NumDynamicVerts_InUse = DynamicEdges.Num() * 2;
	const INT Waste = NumDynamicVerts - NumDynamicVerts_InUse;
	
	// if we have too many verts lying around, do a prune pass
	if( Waste > MAX_WASTE || NumDynamicVerts > MAX_VERT_THRESH )
	{
		static TArray<FMeshVertex> VertsInUse;
		VertsInUse.Reset();
		static TMap< VERTID, VERTID > OldIdxToNewIdxMap;
		OldIdxToNewIdxMap.Reset();

		// loop through all dynamic edges and store off the verts they're using
		for(DynamicEdgeList::TIterator It(DynamicEdges); It; ++It )
		{
			FNavMeshCrossPylonEdge* Edge = It.Value();

			// 0 
			VERTID EdgeVertId = Edge->Vert0;
			VERTID* pNewId = OldIdxToNewIdxMap.Find(EdgeVertId);
			INT NewIdx = MAXVERTID;
			if( pNewId != NULL )
			{
				NewIdx = *pNewId;
			}
			else
			{
				NewIdx = VertsInUse.AddItem(Verts(EdgeVertId));
			}
			Edge->Vert0 = StaticVertCount+NewIdx;

			// 1
			EdgeVertId = Edge->Vert1;
			pNewId = OldIdxToNewIdxMap.Find(EdgeVertId);
			NewIdx = MAXVERTID;
			if( pNewId != NULL )
			{
				NewIdx = *pNewId;
			}
			else
			{
				NewIdx = VertsInUse.AddItem(Verts(EdgeVertId));
			}
			Edge->Vert1 = StaticVertCount+NewIdx;
		}

		Verts.Remove(StaticVertCount,  NumDynamicVerts );
		Verts.Append(VertsInUse);

	}
}


/**
 * removes a dynamic cross pylon edge from this mesh.  Also handles removing corresponding edges from other meshes
 * @param Edge - the edge to be removed
 */
void UNavigationMeshBase::RemoveDynamicCrossPylonEdge( FNavMeshCrossPylonEdge* Edge )
{
	FNavMeshPolyBase* OtherPoly = NULL;
	UNavigationMeshBase* OtherMesh = NULL;

	check(Edge->NavMesh == this);


	// remove it from the edgemap FIRST so if we get back in here for the same edge we know not to try again
	INT MappingsRemoved = 0;

	WORD PolyID = (IsSubMesh()) ? Edge->Poly0Ref.GetSubPolyId() : Edge->Poly0Ref.GetTopLevelPolyId();
	MappingsRemoved += DynamicEdges.RemovePair(PolyID,Edge);

	PolyID = (IsSubMesh()) ? Edge->Poly1Ref.GetSubPolyId() : Edge->Poly1Ref.GetTopLevelPolyId();
	MappingsRemoved += DynamicEdges.RemovePair(PolyID,Edge);

	// if this edge was not linked to the mesh, then it's probably already cleaned up.. bail
	if(MappingsRemoved == 0)
	{
		return;
	}

	// remove any reference to this edge from our obstacle mesh
	if(Edge->ObstaclePolyID != MAXWORD)
	{
		//  [7/25/2012 shawn.harris] I don't believe this data is ever set.
		GetPylon()->ObstacleMesh->DynamicEdges.RemovePair(Edge->ObstaclePolyID,Edge);
		// MT-> this sucks, is prone to badness.... maybe good enough for prototype ;)
		//  [7/25/2012 shawn.harris] - The way in which the PolyEdges are used for obstacle meshes are  
		//ambiguous.  Cross pylon edges add MAXWORD and MAXWORD-1 while LinkToObstacleGeo and AddDropDownEdge adds the actual edge index 
		//to PolyEdges.  Need to talk with Steve S. about this. 
 		FNavMeshPolyBase* ObstaclePoly = GetPylon()->ObstacleMesh->GetPolyFromId(Edge->ObstaclePolyID);
 		ObstaclePoly->PolyEdges.RemoveItem(MAXWORD-1); 
		ObstaclePoly->PolyEdges.RemoveItem(MAXWORD);	  
 	}

	FNavMeshPolyBase* DestPoly = Edge->Poly1Ref.GetPoly(TRUE);
	if( DestPoly != NULL )
	{
		DestPoly->NavMesh->RemoveEdgeRefOfMesh(Edge);
	}
	// delete the current edge!
	FNavMeshWorld::DestroyEdge(Edge,FALSE);
	
	PruneDynamicVerts();
}

/**
* Copy a poly from another mesh into this one
* @param Poly - other poly to copy
* @return - the new poly in this mesh
*/
FNavMeshPolyBase* UNavigationMeshBase::CopyPolyIntoMesh(const FNavMeshPolyBase* Poly)
{
	TArray<FVector> Verts;
	for(INT VertIdx=0;VertIdx<Poly->PolyVerts.Num();++VertIdx)
	{
		Verts.AddItem(Poly->GetVertLocation(VertIdx,WORLD_SPACE));
	}

	return AddPoly(Verts,Poly->PolyHeight,WORLD_SPACE);
}

/**
 * Add a poly to the pool -- return that new poly 
 * @param inVerts     - A list of FVectors representing this new polygon
 * @param PolyHeight  - (optional) if non-negative, will be used as the height of this new poly, otherwise a height will be calculated 
 * @param bWorldSpace - (optional) if TRUE, treats incoming vert locations as world space locations
 * @param MaxVertSnapHeight - (optional) the max height that verts will be snapped together 
 * @param MaxVertSnap - (optional) the max overall distance a vert can be snapped together (default is GRIDSIZE)
 * @return - the poly just added
 */
FNavMeshPolyBase* UNavigationMeshBase::AddPoly( const TArray<FVector>& inVerts, FLOAT PolyHeight, UBOOL bWorldSpace, FLOAT MaxVertSnapHeight/*=-1.0f*/, FLOAT MaxVertSnapDist/*=-1.0f*/  )
{
	static TArray<VERTID> VertIndices;
	VertIndices.Reset();
	FVector PolyCenter = FVector(0.f);
	checkSlowish(inVerts.Num() > 0);
	for( INT Idx = 0; Idx < inVerts.Num(); Idx++ )
	{
		VERTID NewVert = AddVert(inVerts(Idx),bWorldSpace, MaxVertSnapHeight, FALSE, MaxVertSnapDist);
		if( NewVert == MAXVERTID )
		{
			return NULL;
		}

		INT Index = VertIndices.FindItemIndex(NewVert);
		if( Index != INDEX_NONE )
		{
			// if it's adjacent to the duplicate, just don't add the dupe (probably from very narrow corner)
			if( Index != (VertIndices.Num()-1) && Index != 0 )
			{
				warnf(NAME_Error,TEXT("Attempted to add a poly that had non-adjacent duplicate verts... abort (%s)"), *GetPylon()->GetName());
				return NULL;
			}
		}
		else
		{
			VertIndices.AddItem( NewVert );
		}
	}

	if( VertIndices.Num() < 3 || FNavMeshPolyBase::CalcArea(VertIndices,this) < NAVMESHGEN_MIN_POLY_AREA )
	{
		warnf(NAME_DevNavMeshWarning,TEXT("Poly had < 3 verts or too small of an area after AddVert() calls.. skipping this poly!"));
		return NULL;
	}

	return AddPolyFromVertIndices(VertIndices,PolyHeight);
}


/**
 *	will add a new polygon to the mesh based on a list of pre-existing vertex IDs
 * @param inVertIndices - list of vert IDs to add from
 * @param PolyHeight - height of poly (-1 means calculate height now)
 * @return a ref to the new poly, or NULL if none was created
 */
FNavMeshPolyBase* UNavigationMeshBase::AddPolyFromVertIndices( const TArray<VERTID>& inVertIndices, FLOAT PolyHeight)
{
	checkSlowish(inVertIndices.Num() > 0);
	FNavMeshPolyBase* Poly = new FNavMeshPolyBase(this,inVertIndices, PolyHeight);
	if(Poly->PolyHeight < NAVMESHGEN_ENTITY_HALF_HEIGHT)
	{
		debugf(TEXT("WARNING!: removing poly with invalid poly height.. FindGround and FindCeiling are fighting"));
		delete Poly;
		return NULL;
	}

	Poly->Item = BuildPolys.Num();
	BuildPolys.AddTail( Poly );	

	//if( GetPylon() == NULL || GetPylon()->NavMeshPtr == this )
	{
		AddPolyToOctree(Poly);
	}

	// add claims on verts
	for(INT VertIdx=0;VertIdx<Poly->PolyVerts.Num();VertIdx++)
	{
		Verts(Poly->PolyVerts(VertIdx)).ContainingPolys.AddUniqueItem(Poly);
	}

	if(Poly->IsBorderPoly())
	{
		TArray<FNavMeshPolyBase*> AdjacentPolys;
		Poly->GetAdjacentPolys(AdjacentPolys);
		// add this to the list of border polys
		Poly->SetBorderPoly(TRUE,&AdjacentPolys);
	}

	BoxBounds += Poly->BoxBounds;
	return Poly;
}

/**
 *	internal function which will run simplification steps based on configuration parameters
 * @param bSkipSquareMerge - should be TRUE if htis is a second pass merge (skips expensive merge steps)
 */
INT UNavigationMeshBase::SimplifyMesh(UBOOL bSkipSquareMerge)
{
	INT NumMerged = 0;

	if(!bSkipSquareMerge && ExpansionDoSquareMerge)
	{
		SCOPE_QUICK_TIMERX(SquareMerge,TRUE)
		NumMerged += MergeSquares();
	}
	if(ExpansionDoPolyMerge)
	{
		SCOPE_QUICK_TIMERX(PolyMerge,TRUE)
			NumMerged += MergePolys();
	}

	if(ExpansionDoThreeToTwoMerge)
	{
		SCOPE_QUICK_TIMERX(ThreeToTwo,TRUE)
			NumMerged += ThreeToTwoMerge();
	}


	if( ExpansionDoPolyConcaveMerge && !bSkipSquareMerge )
	{
		// cull out poopy polys that didn't get merged in
		//CullSillyPolys();

		{
			SCOPE_QUICK_TIMERX(ConcaveMerge,TRUE)
			NumMerged += MergePolysConcave();
		}

		if(!ExpansionDoConcaveSlabsOnly)
		{
			SCOPE_QUICK_TIMERX(OverallDecomposition,TRUE)
			ConvexinateMesh();
		}
	}

	return NumMerged;
}

/**
 *	this will run around to all the polys in the mesh and attempt to remove unnecessary verts
 */
void UNavigationMeshBase::SimplifyEdgesOfMesh()
{
	SCOPE_QUICK_TIMERX(MeshEdgeSimplification,TRUE)
	INT VertsRemoved=0;
	for(PolyList::TIterator Itt(BuildPolys.GetTail());Itt;--Itt)
	{
		FNavMeshPolyBase* Poly = *Itt;
		VertsRemoved += SimplifyEdgesOfPoly(Poly);
	}

	debugf(TEXT("Removed %i verts."),VertsRemoved);
}

/**
 * this will keep edges of adjacent polys aligned together and prevent large vertical gaps between adjacent polys
 */
void UNavigationMeshBase::AlignAdjacentPolys()
{
	if( ! ExpansionDoAdjacentPolyVertAlignment )
	{
		return;
	}

	SCOPE_QUICK_TIMERX(AlignAdjacentPolys,TRUE)
	INT VertsAdded=0;
	for(PolyList::TIterator Itt(BuildPolys.GetTail());Itt;--Itt)
	{
		FNavMeshPolyBase* Poly = *Itt;
		VertsAdded += AlignAdjacentEdgesForPoly(Poly);
	}

	debugf(TEXT("Re-Added %i verts."),VertsAdded);
}

/** 
 * snap close internal concave verts! 
 * when merging into concave slabs sometimes you'll get corner verts which are very close to the boundary of the poly
 * and cause tiny sliver cases that are difficult to decompose into convex shapes.
 * this function attempts to find these cases and snap the verts onto the boundary 
 * @param Poly - the poly to snap internal concave verts for
 */
void UNavigationMeshBase::SnapCloseInternalConcaveVerts()
{
	static UBOOL bDoSnappingSnapSnappitySnaps = TRUE;
	SCOPE_QUICK_TIMERX(SnapCloseInternalConcaveVerts,TRUE)

	if( bDoSnappingSnapSnappitySnaps )
	{
		for(PolyList::TIterator Itt(BuildPolys.GetTail());Itt;--Itt)
		{
			FNavMeshPolyBase* Poly = *Itt;
			SnapCloseInternalConcaveVertsForPoly(Poly);
		}
	}
}

/** 
 * snap close internal concave verts! 
 * when merging into concave slabs sometimes you'll get corner verts which are very close to the boundary of the poly
 * and cause tiny sliver cases that are difficult to decompose into convex shapes.
 * this function attempts to find these cases and snap the verts onto the boundary 
 * @param Poly - the poly to snap internal concave verts for
 */

UBOOL IsVertexOnEdge(VERTID VertID, UNavigationMeshBase* Mesh, const TArray<FNavMeshPolyBase*>& ExcludedPolys, UBOOL bEndPointExclusive);
void UNavigationMeshBase::SnapCloseInternalConcaveVertsForPoly(FNavMeshPolyBase* Poly)
{
	static FLOAT MinZSnapHeight_Small = 0.5f;
	static FLOAT MinZSnapHeight_Large = 5.0f;
#define CLOSE_EDGE_TOLERANCE NAVMESHGEN_STEP_SIZE * 0.5f
#define MIN_Z_SNAP_HEIGHT_SMALL MinZSnapHeight_Small
#define MIN_Z_SNAP_HEIGHT_LARGE MinZSnapHeight_Large

	// for each vert, check against all edges.. 
	// if this vert is really close to an edge, snap it to that edge
	FVector CurEdgeVert(0.f), NextEdgeVert(0.f), NextEdgeVert_NoZ(0.f), CurEdgeVert_NoZ(0.f);
	FVector CurrentCheckVert(0.f),CurrentCheckVert_NoZ(0.f);

	FVector Closest(0.f);
	FLOAT T = 0.f;

	TArray<FNavMeshPolyBase*> EList;
	EList.AddItem(Poly);

	TArray<FNavMeshPolyBase*> BlankList;

	for( INT OuterVertIdx=0; OuterVertIdx<Poly->PolyVerts.Num(); ++OuterVertIdx)
	{
		CurrentCheckVert = Poly->GetVertLocation(OuterVertIdx);
		CurrentCheckVert_NoZ = CurrentCheckVert;
		CurrentCheckVert_NoZ.Z = 0.f;

		// if the current vert is on an edge of a poly we are not snapping for, or 
		// this vertex is on the edge 
		if( IsVertexOnEdge(Poly->PolyVerts(OuterVertIdx),this,EList,FALSE) )
		{
			continue;
		}
		
		for(INT InnerVertIdx=0;InnerVertIdx<Poly->PolyVerts.Num();++InnerVertIdx)
		{
			INT NextVertIdx = (InnerVertIdx+1) % Poly->PolyVerts.Num();
			if( InnerVertIdx == OuterVertIdx || NextVertIdx == OuterVertIdx )
			{
				continue;
			}
			CurEdgeVert = Poly->GetVertLocation(InnerVertIdx);
			NextEdgeVert = Poly->GetVertLocation( NextVertIdx );
			CurEdgeVert_NoZ = CurEdgeVert;
			NextEdgeVert_NoZ = NextEdgeVert;
			CurEdgeVert_NoZ.Z = NextEdgeVert_NoZ.Z = 0.f;

			if (  ( CurEdgeVert - NextEdgeVert ).SizeSquared() <= CLOSE_EDGE_TOLERANCE * CLOSE_EDGE_TOLERANCE )
			{
				continue;
			}


			FLOAT Dist = PointDistToSegmentOutT(CurrentCheckVert_NoZ,CurEdgeVert_NoZ,NextEdgeVert_NoZ,Closest,T);
			if ( Dist < CLOSE_EDGE_TOLERANCE 
				&& T > KINDA_SMALL_NUMBER 
				&& T < 1.0f -KINDA_SMALL_NUMBER)
			{
				Closest = CurEdgeVert + (NextEdgeVert-CurEdgeVert)*T;
				FLOAT ZDelta = Abs<FLOAT>(Closest.Z - CurrentCheckVert.Z);
				
				FLOAT Zmin = MinZSnapHeight_Large;
				if ( Dist < 0.1f )
				{
					Zmin = MinZSnapHeight_Small;
				}

				if( ZDelta > Zmin && ZDelta < NAVMESHGEN_MAX_STEP_HEIGHT )
				{
					//GWorld->GetWorldInfo()->DrawDebugLine(CurrentCheckVert,Closest,255,255,0,TRUE);
					VERTID OuterVertID = Poly->PolyVerts(OuterVertIdx);
					VERTID NewVert = AddVert(Closest,TRUE);
					Verts(NewVert) = W2LTransformFVector(Closest);// snap vert to height 

					if( NewVert != OuterVertID )
					{
						Poly->RemoveVertexAtLocalIdx(OuterVertIdx,TRUE);
						Poly->PolyVerts(OuterVertIdx) = NewVert;
						Verts(NewVert).ContainingPolys.AddUniqueItem(Poly);
					}
				}
			}
		}
	}
	

}

/**
 * tries to align edges of the given poly with edges of polys adjacent to it
 * @param Poly - the poly to align
 * @return the number of verts added in the process
 */
#define NAVMESHGEN_EDGE_ALIGN_VERT_DIST NAVMESHGEN_MAX_STEP_HEIGHT*0.25f
#define NAVMESHGEN_EDGE_ALIGN_VERT_DIST_MAX NAVMESHGEN_MAX_STEP_HEIGHT
INT UNavigationMeshBase::AlignAdjacentEdgesForPoly(FNavMeshPolyBase* Poly)
{
	INT VertsAdded=0;
	// grab all verts within bounds of the poly
	static TArray<VERTID> NearVerts;
	NearVerts.Reset();
	GetAllVertsNearPoint(Poly->BoxBounds.GetCenter(),Poly->BoxBounds.GetExtent()+FVector(NAVMESHGEN_EDGE_ALIGN_VERT_DIST_MAX),NearVerts);

	if( Poly->CalcArea() < NAVMESHGEN_MIN_POLY_AREA )
	{
		return 0;
	}

	// loop through edges of poly and find verts in the list which 
	for(INT LocalVertIdx=0;LocalVertIdx<Poly->PolyVerts.Num();++LocalVertIdx)
	{
		const FVector ThisVert = Verts( Poly->PolyVerts(LocalVertIdx) );
		INT NextIDX = (LocalVertIdx+1) % Poly->PolyVerts.Num();
		const FVector NextVert = Verts( Poly->PolyVerts( NextIDX));
		const FVector ThisVertNoZ = FVector(ThisVert.X,ThisVert.Y,0.f);
		const FVector NextVertNoZ = FVector(NextVert.X,NextVert.Y,0.f);	


		for(INT VertIdx=0;VertIdx<NearVerts.Num();++VertIdx)
		{
			const FVector VertLoc = Verts(NearVerts(VertIdx));
			const FVector VertLocNoZ = FVector(VertLoc.X,VertLoc.Y,0.f);	
			if( NearVerts(VertIdx) == Poly->PolyVerts(LocalVertIdx) || NearVerts(VertIdx) == Poly->PolyVerts(NextIDX) )
			{
				continue;
			}

			if( (ThisVertNoZ-VertLocNoZ).Size() < EDGE_VERT_TOLERANCE || (NextVertNoZ-VertLocNoZ).Size() < EDGE_VERT_TOLERANCE )
			{
				continue;
			}

			FLOAT T=0.f;
			FVector Closest(0.f);
			FLOAT NoZDist = PointDistToSegmentOutT(VertLocNoZ,ThisVertNoZ,NextVertNoZ,Closest,T);
			
			FLOAT MinVerticalDist = NAVMESHGEN_EDGE_ALIGN_VERT_DIST;
			FLOAT MaxVerticalDist = NAVMESHGEN_EDGE_ALIGN_VERT_DIST_MAX;
			// if this vert is close to us on the horizontal plane
			if ( NoZDist < EDGE_VERT_TOLERANCE )
			{
				if( NoZDist < KINDA_SMALL_NUMBER )
				{
					MaxVerticalDist *= 2.0f;
				}

				// check its vertical distance 
				const FVector ClosestPtWithZ = ThisVert + (NextVert-ThisVert) * T;
				FLOAT VertDist = Abs<FLOAT>(VertLoc.Z - ClosestPtWithZ.Z);

				if( VertDist > MinVerticalDist && VertDist < MaxVerticalDist)
				{
					// get an accurate ground placement if possible
					FCheckResult Hit(1.0f);
					if( GetPylon()->FindGround(VertLoc,Hit,FPathBuilder::GetScout(),0) )
					{
						FLOAT CurrentZ = Poly->NavMesh->Verts(NearVerts(VertIdx)).Z;
						if( Abs<FLOAT>(CurrentZ - Hit.Location.Z) < MaxVerticalDist )
						{
							FVector HitLoc = Hit.Location;
							HitLoc = Poly->NavMesh->W2LTransformFVector(HitLoc);
							Poly->NavMesh->Verts(NearVerts(VertIdx)).Z = HitLoc.Z;
						}
					}
							
					// insert this vert into ourself
					++VertsAdded;
					// insert vert into poly so it stays aligned with adjacent guys
					Poly->PolyVerts.InsertItem(NearVerts(VertIdx),NextIDX);
					// make sure new vert knows it's being ref'd by this poly
					Poly->NavMesh->Verts(NearVerts(VertIdx)).ContainingPolys.AddUniqueItem(Poly);
					--LocalVertIdx;
					// remove ref to vert since it's already in the poly now
					break;
				}
			}
		}
	}


	return VertsAdded;
}


/**
 * this will triangulate all polys in the mesh.  First by generating a tri list, and then removing all old polys and adding triangles
 */
void UNavigationMeshBase::TriangulateMesh()
{
	for(PolyList::TIterator Itt(BuildPolys.GetTail());Itt;)
	{
		TArray<VERTID> VertexBuffer;
		FNavMeshPolyBase* Poly = *Itt;
		--Itt;
		TriangulatePoly(Poly,VertexBuffer);
		FLOAT PolyHeight = Poly->GetPolyHeight();

		RemovePoly(Poly);

		for(INT TriIdx=0;TriIdx<VertexBuffer.Num();TriIdx += 3)
		{
			TArray<VERTID> NewPoly;
			NewPoly.AddItem(VertexBuffer(TriIdx));
			NewPoly.AddItem(VertexBuffer(TriIdx+1));
			NewPoly.AddItem(VertexBuffer(TriIdx+2));

			AddPolyFromVertIndices(NewPoly,PolyHeight);
		}
	}

}

/**
 *	ConvexinateMesh
 *  Decomposes concave slabs into convex polys (calls DecomposePolyToConvexPrimitives on each poly, adding the resulting polys and removing the concave one)
 *
 */
void UNavigationMeshBase::ConvexinateMesh()
{
	static INT   DebugClip = 0;
	static INT   DebugMaxConvexCount = 0;
	INT Count=0;
	INT Total = BuildPolys.Num();
	for(PolyList::TIterator Itt(BuildPolys.GetHead());Itt;)
	{
		if(Count++ >= DebugMaxConvexCount && DebugMaxConvexCount>0)
		{
			break;
		}
		FNavMeshPolyBase* Poly = *Itt;		
		++Itt;

		if(Poly->PolyVerts.Num() < 3)
		{
			RemovePoly(Poly);
			continue;
		}

		// pop status to keep editor responsive
		GWarn->StatusUpdatef( Count, Total, TEXT("Simplifying mesh"));
		INT NumConvexPolys = DecomposePolyToConvexPrimitives(Poly, -1);
		////////////////////////////////////
	}

}

/** 
 * find polys adjacent to this one who share exactly one vertex
 * (used in 3->2 merging)
 * @param ChkPoly - poly we're trying to find friends for
 * @param out_AdjacentPolys - outpout param filled with adjacent polys who share one vert
 * @param out_SharedVertexPoolIndexes - verts which are shared exactly once between polys
 * @return TRUE If any were found
 */
UBOOL UNavigationMeshBase::FindAdjacentPolysSharingExactlyOneVertex( FNavMeshPolyBase* ChkPoly, TArray<FNavMeshPolyBase*>& out_AdjacentPolys, TArray<VERTID>& out_SharedVertexPoolIndexes )
{

	// -> for each of the incoming polygon's verts
	for( INT VertIdx = 0; VertIdx < ChkPoly->PolyVerts.Num(); VertIdx++ )
	{
		FMeshVertex& Vert = Verts(ChkPoly->PolyVerts(VertIdx));

		// -> for each poly that shares this vert
		for(INT SharingPolyIdx=0;SharingPolyIdx<Vert.ContainingPolys.Num();SharingPolyIdx++)
		{
			FNavMeshPolyBase* SharingPoly = Vert.ContainingPolys(SharingPolyIdx);
			INT NumSharedVerts=0;
			VERTID SharedVertexPoolIndex = MAXVERTID;
			
			if(SharingPoly==ChkPoly)
			{
				continue;
			}

			// count how many shared verts there are
			for(INT SharingPolyVertIdx=0;SharingPolyVertIdx<SharingPoly->PolyVerts.Num();SharingPolyVertIdx++)
			{
				FMeshVertex& OtherVert = Verts(SharingPoly->PolyVerts(SharingPolyVertIdx));
				if(OtherVert.ContainingPolys.ContainsItem(ChkPoly))
				{
					NumSharedVerts++;
					SharedVertexPoolIndex = ChkPoly->PolyVerts(VertIdx);
				}
			}

			if(NumSharedVerts==1)
			{
				out_AdjacentPolys.AddItem(SharingPoly);
				out_SharedVertexPoolIndexes.AddItem(SharedVertexPoolIndex);
			}		

		}

	}

	return (out_AdjacentPolys.Num() > 0);
}


/**
 *	finds a polygon which is adjacent to both polya and polyb
 * (used in 3->2 merging)
 * @param SharedPoolIndex - vertex shared between polya and polyb
 * @param out_AdjacentPoly - out param stuffed with poly that is adjacent to A and B
 * @return TRUE if a poly is found
 */
UBOOL UNavigationMeshBase::FindAdjacentPolyToBothPolys( VERTID SharedPoolIndex, FNavMeshPolyBase* PolyA, FNavMeshPolyBase* PolyB, FNavMeshPolyBase*& out_AdjacentPoly )
{
	TArray<FNavMeshPolyBase*> AdjA, AdjB;
	PolyA->GetAdjacentPolys(AdjA);
	PolyB->GetAdjacentPolys(AdjB);

	INT NumSharedAdjPolys = 0;
	for( INT A = 0; A < AdjA.Num(); A++ )
	{
		FNavMeshPolyBase* TestPoly = AdjA(A);
		if( AdjB.FindItemIndex(TestPoly) >= 0 )
		{
			// Not a valid PolyC if it shares the same vert shared by A&B
			if( TestPoly->PolyVerts.ContainsItem(SharedPoolIndex) )
				continue;

			// For each edge in TestPoly
			for( INT TestEdgeIdx = 0; TestEdgeIdx < TestPoly->PolyVerts.Num(); TestEdgeIdx++ )
			{
				FVector TestEdgeDir = (GetVertLocation(TestPoly->GetAdjacentVertPoolIndex(TestEdgeIdx,1),LOCAL_SPACE)-GetVertLocation(TestPoly->PolyVerts(TestEdgeIdx),LOCAL_SPACE)).SafeNormal();

				// Check if there is a parallel edge in A
				for( INT EdgeAIdx = 0; EdgeAIdx < PolyA->PolyVerts.Num(); EdgeAIdx++ )
				{
					VERTID A1 = PolyA->PolyVerts(EdgeAIdx);
					VERTID A2 = PolyA->GetAdjacentVertPoolIndex(EdgeAIdx,1);
					FVector ADir = (GetVertLocation(A2,LOCAL_SPACE)-GetVertLocation(A1,LOCAL_SPACE)).SafeNormal();

					// If edge on A is parallel to  test edge
					FLOAT TestDotA = TestEdgeDir|ADir;
					if( Abs(TestDotA)+KINDA_SMALL_NUMBER < 1.f )
						continue;

					// Check if there is a parallel edge in B
					for( INT EdgeBIdx = 0; EdgeBIdx < PolyB->PolyVerts.Num(); EdgeBIdx++ )
					{
						VERTID B1 = PolyB->PolyVerts(EdgeBIdx);
						VERTID B2 = PolyB->GetAdjacentVertPoolIndex(EdgeBIdx,1);
						FVector BDir = (GetVertLocation(B2,LOCAL_SPACE)-GetVertLocation(B1,LOCAL_SPACE)).SafeNormal();

						FLOAT TestDotB = TestEdgeDir|BDir;
						if( Abs(TestDotB)+KINDA_SMALL_NUMBER < 1.f )
							continue;

						// Check if parallel edges share one vert
						if( A1 == B1 || A1 == B2 || A2 == B1 || A2 == B2 )
						{
							out_AdjacentPoly = TestPoly;
							return TRUE;
						}
					}
				}
			}
		}
	}

	return FALSE;
}

/**
 *	given two polys which match other criteria, pick one that is going to get split for 3->2 merge
 * (used during 3->2 merging)
 * @param PolyA - first poly to choose from
 * @param PolyB - second poly to choose from 
 * @param PolyC - third poly to choose from 
 * @param out_PolyToKeep - poly that is not getting split or merged
 * @param out_SplitIndex - index of vert we're splitting at 
 * @reutrn TRUE if successful
 */
UBOOL UNavigationMeshBase::ChoosePolyToSplit( FNavMeshPolyBase* PolyA, FNavMeshPolyBase* PolyB, FNavMeshPolyBase* PolyC, VERTID SharedPoolIndex, FNavMeshPolyBase*& out_PolyToSplit, FNavMeshPolyBase*& out_PolyToKeep, VERTID& out_SplitIndex )
{
	INT SharedLocalIndexA = PolyA->PolyVerts.FindItemIndex(SharedPoolIndex);
	INT SharedLocalIndexB = PolyB->PolyVerts.FindItemIndex(SharedPoolIndex);

	// Determine a vertex which is shared by A&B, and adjacent to the shared vertex, but not shared with C
	VERTID OptionA=0;		

	if(PolyC->PolyVerts.ContainsItem(PolyA->GetAdjacentVertPoolIndex(SharedLocalIndexA,1)))
	// if the adjacent vert in the positive direction is shared by C, then we want the adjacent vert in the other direction
	{
		OptionA = PolyA->GetAdjacentVertPoolIndex(SharedLocalIndexA,-1);
	}
	else
	{
		OptionA = PolyA->GetAdjacentVertPoolIndex(SharedLocalIndexA,1);
	}

	VERTID OptionB = 0;
	if(PolyC->PolyVerts.ContainsItem(PolyB->GetAdjacentVertPoolIndex(SharedLocalIndexB,1)))
	// if the adjacent vert in the positive direction is shared by C, then we want the adjacent vert in the other direction
	{
		OptionB = PolyB->GetAdjacentVertPoolIndex(SharedLocalIndexB,-1);
	}
	else
	{
		OptionB = PolyB->GetAdjacentVertPoolIndex(SharedLocalIndexB,1);
	}

	if( (GetVertLocation(OptionA,LOCAL_SPACE)-GetVertLocation(SharedPoolIndex,LOCAL_SPACE)).SizeSquared() < (GetVertLocation(OptionB,LOCAL_SPACE)-GetVertLocation(SharedPoolIndex,LOCAL_SPACE)).SizeSquared() )
	{
		out_PolyToKeep	= PolyA;
		out_PolyToSplit = PolyB;
		out_SplitIndex  = OptionA;
	}
	// Otherwise, B used to split A...
	else
	{
		out_PolyToKeep	= PolyB;
		out_PolyToSplit = PolyA;
		out_SplitIndex  = OptionB;
	}

	return TRUE;
}

/** 
 * calculates where to insert a new vertex in the poly being split for 3->2 merge
 * @param PolyToSplit - poly we're splitting
 * @param SplitPoolIndex - index of other vert we're splitting on
 * @param SplitDir - direction to split
 * @Param out_SplitPt - out param stuffed with the location of the split
 * @return TRUE if succesful
 */
UBOOL UNavigationMeshBase::CalcSplitVertexLocation( FNavMeshPolyBase* PolyToSplit, VERTID SplitPoolIndex, FVector& SplitDir, FVector& out_SplitPt )
{
	// Find intersection on poly that is being split
	FVector SplitStart = GetVertLocation(SplitPoolIndex,LOCAL_SPACE);
	FVector SplitEnd   = SplitStart + SplitDir * 4096.f;
	FVector SplitPt(0.f);
	for( INT Idx = 0; Idx < PolyToSplit->PolyVerts.Num(); Idx++ )
	{
		INT OtherIdx = PolyToSplit->GetAdjacentVertexIndex(Idx,1);

		FVector E1 = GetVertLocation(PolyToSplit->PolyVerts(Idx),LOCAL_SPACE);
		FVector E2 = GetVertLocation(PolyToSplit->PolyVerts(OtherIdx),LOCAL_SPACE);

		FVector Pt1(0.f);
		FVector Pt2(0.f);
		SegmentDistToSegmentSafe( SplitStart, SplitEnd, E1, E2, Pt1, Pt2 );

		if( (Pt1-Pt2).IsNearlyZero() && !(Pt1-SplitStart).IsNearlyZero() && !(Pt1-SplitEnd).IsNearlyZero() )
		{
			out_SplitPt=Pt1;
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * will return TRUE If the passed location should be considered on segment
 * @param Vertloc - vertex location to test against segment
 * @param SegPt0 - first segment point
 * @param SegPt1 - second segment point
 * @param bEndPointExclusive - when this is true, vertlocations that are on top of either segment point will be treated as not on edge
 * @return TRUE If vertloc is on the segment
 */
#define VERTEX_ON_EDGE_TOLERANCE 0.01f
#define VERTEX_ON_EDGE_VERTICAL_TOLERANCE (NAVMESHGEN_MAX_STEP_HEIGHT*0.5f) 
UBOOL IsVertexOnEdgeSegment(const FVector& VertLoc, const FVector& SegPt0, const FVector& SegPt1, UBOOL bEndPointExclusive, FLOAT Tolerance=-1.0f)
{
	if (Tolerance < 0.f)
	{
		Tolerance = VERTEX_ON_EDGE_TOLERANCE;
	}

	const FVector CurVertNoZ = FVector(SegPt0.X,SegPt0.Y,0.f);
	const FVector NextVertNoZ = FVector(SegPt1.X,SegPt1.Y,0.f);
	const FVector VertLocNoZ = FVector(VertLoc.X,VertLoc.Y,0.f);


	FVector Closest(0.f);
	FLOAT T=0.f;
	FLOAT Dist2D = PointDistToSegmentOutT(VertLocNoZ,CurVertNoZ,NextVertNoZ,Closest,T);
	FVector ClosestWithZ = SegPt0 + (SegPt1-SegPt0)*T;
	FLOAT VerticalDist = Abs<FLOAT>(ClosestWithZ.Z - VertLoc.Z);

	if(Dist2D < Tolerance && VerticalDist < VERTEX_ON_EDGE_VERTICAL_TOLERANCE)
	{
		if(!bEndPointExclusive || 
			( !appIsNearlyEqual((FLOAT)0.f,(FLOAT)T,(FLOAT)KINDA_SMALL_NUMBER) && !appIsNearlyEqual((FLOAT)1.f,(FLOAT)T,(FLOAT)KINDA_SMALL_NUMBER) )
			)
		{
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * will return TRUE if the passed vertex is on an edge of a poly other than the passed poly
 * @param VertID - vertex ID of the vert we're checking
 * @param Mesh - mesh the vertex lives in
 * @param ExcludedPolys - polys we want to exclude from the check
 * @param bEndPointExclusive - dictates whether we care if the point is on the ends of the edge or not
 * @return TRUE if the vertex is on an edge
 */

UBOOL IsVertexOnEdge(VERTID VertID, UNavigationMeshBase* Mesh, const TArray<FNavMeshPolyBase*>& ExcludedPolys, UBOOL bEndPointExclusive)
{
	TArray<FNavMeshPolyBase*> Polys;
	const FVector VertLoc = Mesh->GetVertLocation(VertID,WORLD_SPACE);
	const FVector VertLocNoZ = FVector(VertLoc.X,VertLoc.Y,0.f);
	UNavigationHandle::GetAllPolysFromPos(Mesh->GetVertLocation(VertID,WORLD_SPACE),FVector(5.f),Polys,TRUE);

	for(INT PolyIdx=0;PolyIdx<Polys.Num();PolyIdx++)
	{
		FNavMeshPolyBase* CurPoly = Polys(PolyIdx);

		if(ExcludedPolys.ContainsItem(CurPoly))
		{
			continue;
		}

		// for each of this poly's edges
		for(INT PolyVertIdx=0;PolyVertIdx<CurPoly->PolyVerts.Num();PolyVertIdx++)
		{
			const FVector CurVert = CurPoly->NavMesh->GetVertLocation(CurPoly->PolyVerts(PolyVertIdx),WORLD_SPACE);
			const FVector NextVert = CurPoly->NavMesh->GetVertLocation(CurPoly->PolyVerts((PolyVertIdx+1) % CurPoly->PolyVerts.Num()),WORLD_SPACE);

			if ( IsVertexOnEdgeSegment(VertLoc,CurVert,NextVert,bEndPointExclusive) )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
 * this function attempts to figure out if this vertex is forming edges which verts from other polys are on (e.g. can we remove this vert without pulling adjacent polys apart)
 * @param Poly - Poly that owns the vertex we're checking
 * @param LocalVertIdx - index in Poly of the vert we're testing
 */
UBOOL DoesVertexHaveEdgesWithOtherVerts(const FNavMeshPolyBase* Poly, INT LocalVertIdx)
{
	const FVector VertLoc = Poly->GetVertLocation(LocalVertIdx,WORLD_SPACE);

	const INT PrevLocalIdx = Increment(LocalVertIdx,-1,Poly->PolyVerts.Num());
	const INT NextLocalIdx = Increment(LocalVertIdx,1,Poly->PolyVerts.Num());
	
	const FVector PrevEdgePoint = Poly->GetVertLocation(PrevLocalIdx,WORLD_SPACE); 
	const FVector NextEdgePoint = Poly->GetVertLocation(NextLocalIdx,WORLD_SPACE); 

	// edge one (current -> prev)
	FBox EdgeBounds_0(1);
	EdgeBounds_0 += PrevEdgePoint;
	EdgeBounds_0 += VertLoc;
	// edge two (current -> next)
	FBox EdgeBounds_1(1);
	EdgeBounds_1 += NextEdgePoint;
	EdgeBounds_1 += VertLoc;


	TArray< FNavMeshPolyBase* > Polys;
	UNavigationHandle::GetAllPolysFromPos(EdgeBounds_0.GetCenter(),EdgeBounds_0.GetExtent(),Polys,TRUE);
	UNavigationHandle::GetAllPolysFromPos(EdgeBounds_1.GetCenter(),EdgeBounds_1.GetExtent(),Polys,TRUE);
	//MT->Polys may have dupes in it here.. 

	UNavigationHandle::GetAllPolysFromPos(EdgeBounds_0.GetCenter(),EdgeBounds_0.GetExtent(),Polys,TRUE);
	UNavigationHandle::GetAllPolysFromPos(EdgeBounds_1.GetCenter(),EdgeBounds_1.GetExtent(),Polys,TRUE);


	FVector CurrentTestVetLoc(0.f);
	for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
	{
		const FNavMeshPolyBase* CurPoly = Polys(PolyIdx);
		if( CurPoly == Poly )
		{
			continue;
		}
	
		// for each one of this poly's verts
		for(INT VertIdx=0;VertIdx<CurPoly->PolyVerts.Num();++VertIdx)
		{
			CurrentTestVetLoc = CurPoly->GetVertLocation(VertIdx,WORLD_SPACE);
			if( IsVertexOnEdgeSegment(CurrentTestVetLoc,PrevEdgePoint,VertLoc,TRUE) || 
				IsVertexOnEdgeSegment(CurrentTestVetLoc,NextEdgePoint,VertLoc,TRUE) )
			{
				return TRUE;
			}
		}
	}

	return FALSE;	
}

/** 
 * determine if two polygons are of compatible height with each other such that they can be merged
 * @param Poly1 - first poly to consider'
 * @param Poly2 - second poly to consider
 * @return TRUE If polys are compatible for a merge
 */
UBOOL PolysAreCompatibleHeight(FNavMeshPolyBase* Poly1, FNavMeshPolyBase* Poly2)
{
	if(Abs<FLOAT>(Poly1->GetPolyHeight() - Poly2->GetPolyHeight()) < NAVMESHGEN_POLY_HEIGHT_MERGE_THRESH)
	{
		return TRUE;
	}

	return FALSE;
}

/**
 * returns TRUE If the passed point is ont he segment formed bey Pt0,Pt1
 * @param Point - point to check
 * @param SegPt0 - first point of segment
 * @param SegPt1 - second point of segment
 * @Param bEndPointExclusive - don't count it if the point is directly on an end point of the segment
 * @return TRUE if Point is on the segment
 */
UBOOL IsPointOnSegment(const FVector& Point, const FVector& SegPt0, const FVector& SegPt1, UBOOL bEndPointExclusive)
{
	FVector Closest(0.f);
	FLOAT Dist = PointDistToSegment(Point,SegPt0,SegPt1,Closest);
	
	
	if(bEndPointExclusive)
	{
		return (Dist < EDGE_VERT_TOLERANCE && !(Closest-SegPt0).IsNearlyZero() && !(Closest-SegPt1).IsNearlyZero());
	}
	else
	{
		return (Dist < EDGE_VERT_TOLERANCE);
	}
}



/** finds opportunities to merge from 3 polys to 2 polys */
#define DEBUG_THREETOTWOMERGE 0
INT UNavigationMeshBase::ThreeToTwoMerge()
{
	if( ExpansionDoConcaveSlabsOnly )
	{
		return 0;
	}

	INT Result = 0;

	// Setup a counter for polys that fail to merge after being split... 
	// avoids infinite loop when we start over on the list and hit the same one again and again
	TMap<FNavMeshPolyBase*,INT>	FailedThreeToTwoMergeCount;

	for(PolyList::TIterator Itt(BuildPolys.GetTail());Itt;)
	{
		FNavMeshPolyBase* PolyA = *Itt;
		FNavMeshPolyBase* PolyB = NULL;
		FNavMeshPolyBase* PolyC = NULL;
		--Itt;

		TArray<FNavMeshPolyBase*> AdjacentPolysWithOneVertShared;
		TArray<VERTID>			  AdjacentSharedVertexIndexes;

		// grab a list of all the polys which share exactly one vertex with this poly
		if( !FindAdjacentPolysSharingExactlyOneVertex( PolyA,AdjacentPolysWithOneVertShared,AdjacentSharedVertexIndexes ) )
		{
			continue;
		}

		// ** loop through add the polys that share exactly one vertex, and see if there is another poly adjacent to both
		for(INT AdjacentIdx=0;AdjacentIdx<AdjacentPolysWithOneVertShared.Num();AdjacentIdx++)
		{
			// Find adjacent nodes that share only one vertex (A&B)

			VERTID SharedPoolIndex = AdjacentSharedVertexIndexes(AdjacentIdx);
			PolyB = AdjacentPolysWithOneVertShared(AdjacentIdx);

			VERTID SharedAIndex = PolyA->PolyVerts.FindItemIndex(SharedPoolIndex);
			VERTID SharedBIndex = PolyB->PolyVerts.FindItemIndex(SharedPoolIndex);

			// Find another node that is adjacent to both (C)
			if( !FindAdjacentPolyToBothPolys(SharedPoolIndex, PolyA, PolyB, PolyC) )
			{
				// nothing is adjacent to both, move on to next
				continue;
			}

			// sanity check the polys before proceeding
			if(!IsConvex(PolyA->PolyVerts,0.f) || !IsConvex(PolyB->PolyVerts,0.f) || !IsConvex(PolyC->PolyVerts,0.f))
			{
				continue;
			}

			// ensure these polys are of similar height ( don't merge polys of disperate height! )
			if(!PolysAreCompatibleHeight(PolyA,PolyB) || !PolysAreCompatibleHeight(PolyA,PolyC) || !PolysAreCompatibleHeight(PolyC,PolyB))
			{
				continue;
			}

			FLOAT NewHeight = PolyA->GetPolyHeight() + PolyB->GetPolyHeight() + PolyC->GetPolyHeight();
			NewHeight /= 3.0f;

			// Determine which poly to split and which to keep
			FNavMeshPolyBase* PolyToSplit = NULL;
			FNavMeshPolyBase* PolyToKeep  = NULL;
			VERTID SplitPoolIndex = 0;
			if( !ChoosePolyToSplit( PolyA, PolyB, PolyC, SharedPoolIndex, PolyToSplit, PolyToKeep, SplitPoolIndex ) )
			{
				continue;
			}

			// If PolyToKeep has failed too many times... skip it
			INT PolyToKeepFailCount = FailedThreeToTwoMergeCount.FindRef( PolyToKeep );
			if( PolyToKeepFailCount > PolyToKeep->PolyVerts.Num() )
			{
// 				PolyToSplit->DrawPoly( GWorld->PersistentLineBatcher, FColor(255,0,0) ); //red
// 				PolyToKeep->DrawPoly( GWorld->PersistentLineBatcher, FColor(0,255,0) ); //green
// 				PolyC->DrawPoly( GWorld->PersistentLineBatcher, FColor(0,0,255) ); //blue

				continue;
			}

			// determine the plane along which to split the chosen poly
			INT SplitLocalIndex = PolyToKeep->PolyVerts.FindItemIndex(SplitPoolIndex);
			VERTID AdjToSplitLocalIndex = 0;
			if(PolyToKeep->GetAdjacentVertPoolIndex(SplitLocalIndex,1)==SharedPoolIndex)
			{
				AdjToSplitLocalIndex = PolyToKeep->GetAdjacentVertexIndex(SplitLocalIndex,-1);
			}
			else
			{
				AdjToSplitLocalIndex = PolyToKeep->GetAdjacentVertexIndex(SplitLocalIndex,1);
			}
		
			VERTID AdjToSplitPoolIndex = PolyToKeep->PolyVerts(AdjToSplitLocalIndex);

			FVector SplitDir = (GetVertLocation(SplitPoolIndex,LOCAL_SPACE)-GetVertLocation(AdjToSplitPoolIndex,LOCAL_SPACE)).SafeNormal();

			// find the location along the splitting plane for the new vertex
			FVector SplitPt(0.f);
			if( !CalcSplitVertexLocation( PolyToSplit, SplitPoolIndex, SplitDir, SplitPt ) )
			{
				continue;
			}


			VERTID AddedPoolIndex = AddVert(SplitPt,LOCAL_SPACE);
			if( AddedPoolIndex == MAXVERTID )
			{
				return 0;
			}
			
			// splitvertlist is the just a copy of the verts of the original polys to be split (.. not modified)
			TArray<VERTID> SplitVertList = PolyToSplit->PolyVerts;

			// Create new polys
			// create the first of the new polys being split off from the former 'polytosplit'.  
			// Starting with the shared vertex already existing, and ending once we find the edge which the new vertex
			// is on (the edge which is being split)
			TArray<VERTID> Poly1Verts;

			Poly1Verts.AddItem(SharedPoolIndex);

			INT SharedLocalIdx = SplitVertList.FindItemIndex(SharedPoolIndex);
			INT	OrigSharedLocalIdx = SharedLocalIdx;
			FVector CurVertLocation = GetVertLocation(SplitVertList(SharedLocalIdx),LOCAL_SPACE);
			SharedLocalIdx = (SharedLocalIdx+1)%SplitVertList.Num();
			
			UBOOL bDupeVertsFoundWTFMate=FALSE;

			// loop through verts from poly being split adding them one at a time until we find the edge 
			// the new vertex is on, and stop			
			while( TRUE )
			{
				FVector NextVertLocation = Verts(SplitVertList(SharedLocalIdx));

				// Split location in between cur/next
				if( IsPointOnSegment(SplitPt,CurVertLocation,NextVertLocation,FALSE) )
				{
					break;
				}
				else
				{
					//checkSlowish(!Poly1Verts.ContainsItem(SplitVertList(SharedLocalIdx)));

					if( Poly1Verts.ContainsItem(SplitVertList(SharedLocalIdx)) )
					{
						debugf(TEXT("Already contains vert! %i"),SplitVertList(SharedLocalIdx));
						bDupeVertsFoundWTFMate=TRUE;
						break;
					}

					Poly1Verts.AddItem(SplitVertList(SharedLocalIdx));
					SharedLocalIdx = (SharedLocalIdx+1)%SplitVertList.Num();
					CurVertLocation = NextVertLocation;
				}
			}

			if(bDupeVertsFoundWTFMate)
			{
				debugf(TEXT("ThreeToTwoMerge skipping merge on poly:"));

				for(INT Idx=0;Idx<SplitVertList.Num();++Idx)
				{
					debugf(TEXT("SplitVertList[%i](%i): %s"),Idx,SplitVertList(Idx),*Verts(SplitVertList(Idx)).ToString());
				}
				continue;
			}

			if ( Poly1Verts.ContainsItem(SplitPoolIndex) ) 
			{
				continue;
			}

			Poly1Verts.AddUniqueItem(AddedPoolIndex);
			Poly1Verts.AddItem(SplitPoolIndex);


			// start with the last vert added to poly1, and add the verts to poly2 until we come back around to the original shared
			// vertex
			TArray<VERTID> Poly2Verts;
			while( SharedLocalIdx != OrigSharedLocalIdx )
			{
				//checkSlow(!Poly2Verts.ContainsItem(SplitVertList(SharedLocalIdx)));

				Poly2Verts.AddUniqueItem(SplitVertList(SharedLocalIdx));
				SharedLocalIdx = (SharedLocalIdx+1)%SplitVertList.Num();
			}

			Poly2Verts.AddUniqueItem(SplitPoolIndex);
			Poly2Verts.AddUniqueItem(AddedPoolIndex);

#if DEBUG_THREETOTWOMERGE
			debugf(TEXT("SharedPoolIdx: %d SplitPoolIndex: %d AddedIndex: %d"), SharedPoolIndex, SplitPoolIndex, AddedPoolIndex );
			PolyToKeep->DebugLogVerts( TEXT("PolyToKeep") );
			PolyToSplit->DebugLogVerts( TEXT("PolyToSplit") );
			PolyC->DebugLogVerts( TEXT("PolyC") );

			PolyToSplit->DrawPoly( GWorld->PersistentLineBatcher, FColor(255,0,0) ); //red
			PolyToKeep->DrawPoly( GWorld->PersistentLineBatcher, FColor(0,255,0) ); //green
			PolyC->DrawPoly( GWorld->PersistentLineBatcher, FColor(0,0,255) ); //blue
			GWorld->GetWorldInfo()->DrawDebugBox( GetVertLocation(SharedPoolIndex,LOCAL_SPACE), FVector(5,5,5), 0, 0, 255, TRUE );	//blue
			GWorld->GetWorldInfo()->DrawDebugBox( GetVertLocation(SplitPoolIndex,LOCAL_SPACE), FVector(5,5,5), 255, 255, 255, TRUE ); //white
			GWorld->GetWorldInfo()->DrawDebugLine( GetVertLocation(SplitPoolIndex,LOCAL_SPACE),GetVertLocation(SplitPoolIndex,LOCAL_SPACE)+SplitDir*2048.f,255,255,255,TRUE); //white
			GWorld->GetWorldInfo()->DrawDebugBox( GetVertLocation(AdjToSplitPoolIndex,LOCAL_SPACE), FVector(5,5,5), 128, 128, 128, TRUE ); //gray
			GWorld->GetWorldInfo()->DrawDebugBox( GetVertLocation(AddedPoolIndex,LOCAL_SPACE), FVector(5,5,5), 0, 0, 0, TRUE );	//black
#endif

			// sanity checks of the newly created polys
			UBOOL bConvex = IsConvex(Poly1Verts,0.f) && IsConvex(Poly2Verts,0.f);
			UBOOL bNormalValid = !FNavMeshPolyBase::CalcNormal(Poly1Verts,this).IsNearlyZero(KINDA_SMALL_NUMBER) && !FNavMeshPolyBase::CalcNormal(Poly2Verts,this).IsNearlyZero(KINDA_SMALL_NUMBER);
			UBOOL bAreaValid = (FNavMeshPolyBase::CalcArea(Poly1Verts,this) > EDGE_VERT_TOLERANCE && FNavMeshPolyBase::CalcArea(Poly2Verts,this) > EDGE_VERT_TOLERANCE);
			
			if( !bConvex || !bNormalValid || !bAreaValid )
			{
				continue;
			}

			// if we're about to delete the poly the iterator is pointing at, advance the itterator first
			if(Itt && PolyToSplit == *Itt)
			{
				--Itt;
			}
			RemovePoly( PolyToSplit );
			FNavMeshPolyBase* NewPoly1 = AddPolyFromVertIndices( Poly1Verts,NewHeight );
			FNavMeshPolyBase* NewPoly2 = AddPolyFromVertIndices( Poly2Verts,NewHeight );

#if DEBUG_THREETOTWOMERGE
			NewPoly1->DrawPoly( GWorld->PersistentLineBatcher, FColor(0,0,0), FVector(0,0,20) );
			NewPoly2->DrawPoly( GWorld->PersistentLineBatcher, FColor(255,255,255), FVector(0,0,40) );
			NewPoly1->DebugLogVerts( TEXT("NewPoly1") );
			NewPoly2->DebugLogVerts( TEXT("NewPoly2") );
#endif

			FNavMeshPolyBase* CombinedPoly = TryCombinePolys(NewPoly1, PolyToKeep, SharedPoolIndex, SplitPoolIndex );

#if DEBUG_THREETOTWOMERGE
			CombinedPoly->DrawPoly( GWorld->PersistentLineBatcher, FColor(128,128,128), FVector(0,0,70) );
			CombinedPoly->DebugLogVerts( TEXT("CombinedPoly") );
#endif

			if(CombinedPoly != NULL)
			{
				Result += 1;
				Result += MergePolys();
			}
			else
			{
				// Failsafe for infinitely trying to 3->2 merge the same poly that has failed
				INT PrevFailCount = FailedThreeToTwoMergeCount.FindRef( PolyToKeep );
				FailedThreeToTwoMergeCount.Set( PolyToKeep, ++PrevFailCount );
			}

				// if we got this far it means a poly was removed, so we need to start over to avoid
				// using corrupt data
				Itt = PolyList::TIterator(BuildPolys.GetTail());
				break;
		}// END adjacent poly loop
	}


	return Result;
}

/**
 * tries to find a neighboring (e.g. directly linked) vertex to the one passed which is in the direction passed
 * @param BaseVertIdx - vertex we're trying to find a facing neighbor
 * @param RectVerts - list of VERTEXIDs of the rectangle configuration we're after
 * @param OwningIdx - index of the vertex we're coming from in the owning rectangle (e.g. predecessor vertex)
 * @param Mesh - mesh we're working in
 * @param Dir - Dir (in world space) that we would like to expand in
 * @param OutcontainingPoly - the poly that contains the vertex that we found (if any)
 * @return - ther vertex id of the vert we found in the passe ddirection (if any) (MAXVERTID if none found)
 */
VERTID FindNeighborVertThatFacesDir(VERTID BaseVertIdx, const TArray<VERTID>& RectVerts, VERTID OwningIdx, UNavigationMeshBase* Mesh, const FVector& WS_Dir, FNavMeshPolyBase*& OutContainingPoly)
{
	const FVector Dir = Mesh->WorldToLocal.InverseTransformNormal(WS_Dir);

	FMeshVertex& BaseVert = Mesh->Verts(BaseVertIdx);

	// loop through all polys that share the vert given, and find a neighboring vert in them that is in the direction passed
	for(INT SharingIndex=0;SharingIndex<BaseVert.ContainingPolys.Num();SharingIndex++)
	{
		FNavMeshPolyBase* SharingPoly = BaseVert.ContainingPolys(SharingIndex);

		if(SharingPoly->PolyVerts.Num() != 4)
		{
			continue;
		}

		INT FoundIdx = 0;
		UBOOL bFound = SharingPoly->PolyVerts.FindItem(BaseVertIdx,FoundIdx);
		checkSlowish(bFound);
		

		INT Next = (FoundIdx+1<SharingPoly->PolyVerts.Num()) ? FoundIdx+1 : 0;
		INT Prev = (FoundIdx-1 >= 0) ? FoundIdx-1 : SharingPoly->PolyVerts.Num() - 1;

		VERTID NextSharingVert = SharingPoly->PolyVerts(Next);
		if( ((Mesh->Verts(NextSharingVert) - BaseVert).SafeNormal2D() | Dir) > 0.99f )
		{
			// here we want to make sure that the poly that gets credit for this neighbor is the one which we will be enveloping
			// upon expansion.  If the neighbor vertex is in the current config, its local index in the neighbor polyneeds to be different 
			// than the local index in the owning rectangle.  And if the neighbor vertex is not in the current config, the local indecies 
			// need to match
			UBOOL bVertInOwningRect = RectVerts.FindItem(NextSharingVert,FoundIdx);
			if( ( bVertInOwningRect && FoundIdx != Next) || (!bVertInOwningRect && OwningIdx == Next) )
			{
				OutContainingPoly=SharingPoly;
				return SharingPoly->PolyVerts(Next);
			}
		}

		VERTID PrevSharingVert = SharingPoly->PolyVerts(Prev);

		if( ((Mesh->Verts(PrevSharingVert) - BaseVert).SafeNormal2D() | Dir) > 0.99f)
		{
			// here we want to make sure that the poly that gets credit for this neighbor is the one which we will be enveloping
			// upon expansion.  If the neighbor vertex is in the current config, its local index in the neighbor polyneeds to be different 
			// than the local index in the owning rectangle.  And if the neighbor vertex is not in the current config, the local indecies 
			// need to match
			UBOOL bVertInOwningRect = RectVerts.FindItem(PrevSharingVert,FoundIdx);
			if( ( bVertInOwningRect && FoundIdx != Prev) || (!bVertInOwningRect && OwningIdx == Prev) )
			{
				OutContainingPoly=SharingPoly;
				return SharingPoly->PolyVerts(Prev);
			}
		}
	}

	//debugf(TEXT("Failed to find neighboring vert to %s OwningIdx: %i Dir: %s"),*BaseVert.ToString(),OwningIdx,*Dir.ToString());
	return MAXVERTID;
}

/**
 *	finds the longest edge of the given poly, and returns its length
 * @Param Poly - the poly to find the longest edge of
 * @return - the length of the longest poly
 */
FLOAT GetLongestEdgeLength(FNavMeshPolyBase* Poly)
{
	INT NextIdx =0;
	FLOAT LongestDist=-1.f;
	FLOAT ThisDist =-1.f;
	for(INT PolyVertIdx=0;PolyVertIdx<Poly->PolyVerts.Num();PolyVertIdx++)
	{
		FMeshVertex& Vert = Poly->NavMesh->Verts(Poly->PolyVerts(PolyVertIdx));

		NextIdx = (PolyVertIdx + 1)%Poly->PolyVerts.Num();
		FMeshVertex& NextVert = Poly->NavMesh->Verts(Poly->PolyVerts(NextIdx));

		ThisDist = (Vert-NextVert).Size();
		if(ThisDist > LongestDist)
		{
			LongestDist=ThisDist;
		}
	}

	return ThisDist;
}

/**
 * determines if the two polys are of compantible slope to be merged together 
 * @param Poly1 - first poly to compare
 * @param Poly2 - second poly to compare
 * @param MinDot - optional param to override mindot threshold
 * @return TRUE If they are compatible
 */
UBOOL PolysAreCompatibleSlope(FNavMeshPolyBase* Poly1, FNavMeshPolyBase* Poly2, FLOAT MinDot=-100.f)
{
	if(MinDot < -1.f)
	{
		MinDot = NAVMESHGEN_MERGE_DOT_LARGE_AREA;
	}

	FLOAT Dot = Poly1->CalcNormal() | Poly2->CalcNormal();

	return Dot >= MinDot;
}

/**
 *	adds the passed poly's verts to the passed control point list (in local space)
 * @param ContainingPOly - poly whose verts now need to be control points
 * @param ControlPoints - list of control points we're adding ot
 */
void AddPolyVertsAsControlPoints(FNavMeshPolyBase* ContainingPoly, ControlPointList& ControlPoints )
{

	for(INT PolyVertIdx=0;PolyVertIdx<ContainingPoly->PolyVerts.Num();PolyVertIdx++)
	{
		VERTID CurVertIdx = ContainingPoly->PolyVerts(PolyVertIdx);
		ControlPoints.AddItem(CurVertIdx);
	}
}

/**
 * used to verify that the passed poly has at least one vertex along the edge described by Vert1->Vert2
 * @param Poly Poly to check
 * @param Vert1 - first vert
 * @param Vert2 - second vert 
 * @return TRUE if poly has edge along passed segment
 */
UBOOL PolyHasEdgeAlongSegment(FNavMeshPolyBase* Poly, VERTID Vert1, VERTID Vert2)
{
	FMeshVertex& Vert1Act = Poly->NavMesh->Verts(Vert1);
	FMeshVertex& Vert2Act = Poly->NavMesh->Verts(Vert2);

	for(INT PolyVertIdx=0;PolyVertIdx<Poly->PolyVerts.Num();PolyVertIdx++)
	{
		FMeshVertex& Vert = Poly->NavMesh->Verts(Poly->PolyVerts(PolyVertIdx));
		FVector Closest(0.f);
		if(PointDistToSegment(Vert,Vert1Act,Vert2Act,Closest) < 0.1f)
		{
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * this function will verify that the potential new poly is not too far off its control points (control points are verts of the polys that used to make up this merged poly)
 * this insures against cascading merges ending up in polys horribly far away from the original set they are supposed to represent
 * @param P - center of poly to test against control points
 * @param Norm - normal of poly to test against control points
 * @param ControlPoints - control points to test against the plane specified
 * @param Mesh - mesh containing the poly
 * @param DistThresh - optional, distance threshold above which we will throw out the merge
 */
UBOOL VerifyNewPolyDistanceToControlPoints(const FVector& P, const FVector&  Norm, ControlPointList& ControlPoints, UNavigationMeshBase* Mesh, FLOAT DistThresh=-1.f)
{
	if(DistThresh < 0.f)
	{
		DistThresh = NAVMESHGEN_POLY_MERGE_DIST_THRESH;
	}

	// verify distance relative to Norm between all control points from A->B
	for(INT ControlPtIdx=0;ControlPtIdx<ControlPoints.Num();ControlPtIdx++)
	{
		const FVector ControlPt = Mesh->GetVertLocation(ControlPoints( ControlPtIdx ),LOCAL_SPACE);
	
		FLOAT Dist = Abs<FLOAT>(FPointPlaneDist(ControlPt,P,Norm));

		if(Dist > DistThresh)
		{
			return FALSE;
		}
	}

	return TRUE;
}

/**
 *	tries to expand the passed rectangle configuration in the direction specified
 * @param Vert1 - first vertex ID of the edge we're trying to expand
 * @Param Vert2 - second vertex ID of th eedge we're trying ot expand
 * @param Dir - direction we're trying to expand in
 * @param Mesh - mesh we're living in currently
 * @param CurRectConfig - rectangle configuration we're trying to expand
 * @param LS_ConfigCenter - centroid of current config
 * @param LS_ConfigNormal - nomral of current config
 * @param CtrlPtMap - map of polys to control point lists for them
 * @return TRUE If successful
 */
UBOOL TryExpandRectEdgeInDir(VERTID& Vert1, VERTID& Vert2, const FVector& Dir, UNavigationMeshBase* Mesh, RectangleConfiguration& CurRectConfig, const FVector& LS_ConfigCenter, const FVector& LS_ConfigNormal, ControlPointMap& CtrlPtMap)
{
	// try and push the edge out in the direction given
	FVector Edge = Mesh->Verts(Vert1) - Mesh->Verts(Vert2);
	FLOAT EdgeLen = Edge.Size();
	FVector EdgeDir = Edge/EdgeLen;
	FNavMeshPolyBase* ContainingPoly=NULL;



	INT TopMatchIdx = 0;
	UBOOL bFound = CurRectConfig.Verts.FindItem(Vert2,TopMatchIdx);
	checkSlowish(bFound);
	VERTID NewTopLeftVert = FindNeighborVertThatFacesDir(Vert2,CurRectConfig.Verts,TopMatchIdx,Mesh,Dir,ContainingPoly);
	FNavMeshPolyBase* TopPoly = ContainingPoly;

	if( TopPoly == NULL ||
		CurRectConfig.OrigPoly == NULL ||
		!PolysAreCompatibleSlope(TopPoly,CurRectConfig.OrigPoly) ||
		!PolysAreCompatibleHeight(TopPoly,CurRectConfig.OrigPoly))
	{
		NewTopLeftVert=MAXVERTID;
	}
	else
	{
		ControlPointList* CtrlPtList = CtrlPtMap.Find(TopPoly);
		if(CtrlPtList != NULL && !VerifyNewPolyDistanceToControlPoints(LS_ConfigCenter,LS_ConfigNormal,*CtrlPtList,Mesh))
		{
			NewTopLeftVert=MAXVERTID;
		}
	}

	if(NewTopLeftVert!=MAXVERTID)
	{
		AddPolyVertsAsControlPoints(ContainingPoly,CurRectConfig.ControlPoints);
	}

	INT BottomMatchIdx = 0;
	bFound = CurRectConfig.Verts.FindItem(Vert1,BottomMatchIdx);
	checkSlowish(bFound);
	VERTID NewBottomLeftVert = FindNeighborVertThatFacesDir(Vert1,CurRectConfig.Verts,BottomMatchIdx,Mesh,Dir,ContainingPoly);
	FNavMeshPolyBase* BottomPoly = ContainingPoly;
	if( BottomPoly == NULL || CurRectConfig.OrigPoly == NULL || !PolysAreCompatibleSlope(BottomPoly,CurRectConfig.OrigPoly) || !PolysAreCompatibleHeight(BottomPoly,CurRectConfig.OrigPoly))
	{
		NewBottomLeftVert=MAXVERTID;
	}
	else
	{
		ControlPointList* CtrlPtList = CtrlPtMap.Find(BottomPoly);
		if(CtrlPtList != NULL && !VerifyNewPolyDistanceToControlPoints(LS_ConfigCenter,LS_ConfigNormal,*CtrlPtList,Mesh))
		{
			NewBottomLeftVert=MAXVERTID;
		}
	}


	if(NewBottomLeftVert!=MAXVERTID)
	{
		AddPolyVertsAsControlPoints(ContainingPoly,CurRectConfig.ControlPoints);
	}

	// if we were able to expand the top and bottom edges, loop through and try to link between them
	if(NewTopLeftVert != MAXVERTID && NewBottomLeftVert != MAXVERTID)
	{

		VERTID CurVertTestVert = NewTopLeftVert;

		// if the two verts are in different polys, we need to step along the edge and make sure there's no gap.. otherwise we're good
		if(BottomPoly == TopPoly)
		{
			CurVertTestVert=NewBottomLeftVert;
		}
		else
		{
			while(CurVertTestVert != MAXVERTID 
				&& CurVertTestVert != NewBottomLeftVert 
				&& (Mesh->Verts(CurVertTestVert) - Mesh->Verts(NewTopLeftVert)).SizeSquared() <= EdgeLen * EdgeLen)	
			{
				// if the bottom poly contains the vert we're looking for, then we're done
				if(BottomPoly->PolyVerts.ContainsItem(CurVertTestVert))
				{
					CurVertTestVert=NewBottomLeftVert;
				}
				else
				{
					CurVertTestVert = FindNeighborVertThatFacesDir(CurVertTestVert,CurRectConfig.Verts,(TopMatchIdx-1>=0) ? TopMatchIdx-1 : CurRectConfig.Verts.Num()-1,Mesh,EdgeDir,ContainingPoly);
					if( ContainingPoly == NULL ||
						CurRectConfig.OrigPoly == NULL ||
						! PolysAreCompatibleSlope(ContainingPoly, CurRectConfig.OrigPoly) ||
						! PolyHasEdgeAlongSegment(ContainingPoly,Vert1,Vert2) ||
						! PolysAreCompatibleHeight(ContainingPoly,CurRectConfig.OrigPoly))
					{
						CurVertTestVert=MAXVERTID;
					}
					else
					{
						ControlPointList* CtrlPtList = CtrlPtMap.Find(ContainingPoly);
						if(CtrlPtList != NULL && !VerifyNewPolyDistanceToControlPoints(LS_ConfigCenter,LS_ConfigNormal,*CtrlPtList,Mesh))
						{
							CurVertTestVert=MAXVERTID;
						}
					}


					if(CurVertTestVert!=MAXVERTID)
					{
						AddPolyVertsAsControlPoints(ContainingPoly,CurRectConfig.ControlPoints);
					}
				}
			}
		}



		if(CurVertTestVert==NewBottomLeftVert)
		{
			Vert1 = NewBottomLeftVert;
			Vert2 = NewTopLeftVert;
			return TRUE;
		}
	}


	return FALSE;
}

RectangleConfiguration::RectangleConfiguration(FNavMeshPolyBase* Poly)
{
	checkSlowish(Poly->PolyVerts.Num() ==4);
	OrigPoly = Poly;
	Verts=Poly->PolyVerts;

	AddPolyVertsAsControlPoints(Poly,ControlPoints);
}

FLOAT GetRectAspectRatio( const RectangleConfiguration& RectConfig )
{
	FLOAT SideALen = (RectConfig.OrigPoly->NavMesh->Verts(RectConfig.Verts(0)) - RectConfig.OrigPoly->NavMesh->Verts(RectConfig.Verts(1))).Size();
	FLOAT SideBLen = (RectConfig.OrigPoly->NavMesh->Verts(RectConfig.Verts(1)) - RectConfig.OrigPoly->NavMesh->Verts(RectConfig.Verts(2))).Size();

	return  Min<FLOAT>(SideALen,SideBLen) / Max<FLOAT>(SideALen,SideBLen);
}

FLOAT AspectCoef = 0.5f;
FLOAT AspectInnerCoef = /*6.0f*/1.0f;
FLOAT RectangleGoodnessHeuristic( const RectangleConfiguration& RectConfig )
{
	TArray<VERTID> nonconstcopy = RectConfig.Verts;
	FLOAT Area = FNavMeshPolyBase::CalcArea(nonconstcopy,RectConfig.OrigPoly->NavMesh);

// 	FLOAT AspectRatio = GetRectAspectRatio(RectConfig);
// 
// 	// if below area thresh, just use area straight up
// 	//FLOAT EffectiveAreaThresh = (AreaAspectThresh*NAVMESHGEN_STEP_SIZE) * (AreaAspectThresh*NAVMESHGEN_STEP_SIZE);
// 	FLOAT Multiplier = Clamp<FLOAT>(1.0f + (AspectCoef * appLog2(AspectInnerCoef*AspectRatio)),0.f,1.f);
// 	return Area*Multiplier;
	return Area;
}

UBOOL RectangleConfiguration::operator>(const RectangleConfiguration& Other) const
{
	return RectangleGoodnessHeuristic(*this) > RectangleGoodnessHeuristic(Other);
}
UBOOL RectangleConfiguration::operator==(const RectangleConfiguration& Other) const
{
	if(Verts.Num() != Other.Verts.Num())
	{
		return FALSE;
	}

	// find a common vert
	INT StartIdx=0;
	for(;StartIdx<4;StartIdx++)
	{
		for(INT InnerIdx=0;InnerIdx<4;++InnerIdx)
		{
			if(Verts(StartIdx) == Other.Verts(InnerIdx))
			{
				// now that we found a common idx, start from here and compare the rest
				for(INT Offset=0;Offset<4;++Offset)
				{
					if(Verts((StartIdx+Offset)%4) != Other.Verts((InnerIdx+Offset)%4))
					{
						return FALSE;
					}
				}
				// they all matched!
				return TRUE;
			}
		}

	}

	return (Verts.Num() == 0);
}


/**
 *	will try to expand the rectangle in all the cardinal directions
 * @param Config - current rectangle configuration we're attempting to expand
 * @param Mesh - mesh we are working with
 * @param Open - open list of configurations
 * @param Closed - list of already visited configuratiosn
 * @param CtrlPtMap - map of polys to control point lists for those polys
 * @return TRUE If expansion was succesful in at least one direction
 */
typedef TLookupMap<RectangleConfiguration> RectConfigList;
UBOOL ExpandRectConfigAllDirs(RectangleConfiguration& Config, UNavigationMeshBase* Mesh, RectConfigList& Open, RectConfigList& Closed, ControlPointMap& CtrlPtMap)
{

	// these need to be in the following order due to CW nature of winding: E,S,W,N
	FVector Dirs[] = {FVector(0.f,1.f,0.f), FVector(-1.f,0.f,0.f), FVector(0.f,-1.f,0.f), FVector(1.f,0.f,0.f)};
	
	UBOOL bExpandedInAtLeastOneDir = FALSE;

	const FVector LS_Ctr = FNavMeshPolyBase::CalcCenter(Config.Verts,Mesh,LOCAL_SPACE);
	const FVector LS_Norm = FNavMeshPolyBase::CalcNormal(Config.Verts,Mesh,LOCAL_SPACE);

	for(INT Idx=0;Idx<4;Idx++)
	{
		// create a new config for this direction
		
		RectangleConfiguration NewConfig(Config);

		if(TryExpandRectEdgeInDir(NewConfig.Verts(Idx),NewConfig.Verts((Idx+1)%4),Dirs[Idx],Mesh,NewConfig,LS_Ctr,LS_Norm,CtrlPtMap))
		{
			UBOOL bOnOpen = Open.Find(NewConfig) != NULL;
			UBOOL bOnClosed = Closed.Find(NewConfig) != NULL;

			if(!bOnOpen && !bOnClosed)
			{
				// check to see if the new poly is close enough to all our control points
				FVector Norm = FNavMeshPolyBase::CalcNormal(NewConfig.Verts,Mesh,LOCAL_SPACE);
				FVector Ctr = FNavMeshPolyBase::CalcCenter(NewConfig.Verts,Mesh,LOCAL_SPACE);
				if( VerifyNewPolyDistanceToControlPoints(Ctr,Norm,NewConfig.ControlPoints,Mesh) )
				{
					bExpandedInAtLeastOneDir=TRUE;
					Open.AddItem(NewConfig);
				}
			}
		}
	}

	return bExpandedInAtLeastOneDir;
}

/**
 * takes the control points for two polygons, throws them into one list, and then adds that new list to the controlpointmap keyed on
 * new poly
 * @param OldPoly1 - first poly to combine from
 * @param OldPoly2 - second poly to combine from
 * @param NewPoly  - poly to be tied to the new list
 * @param CtrlPtMap - map to set the data in
 * @note it is NOT Safe to dereference the oldpoly pointers as they will be deleted by now, they're just keys into the Map
 */
void CombineControlPoints(FNavMeshPolyBase* OldPoly1, FNavMeshPolyBase* OldPoly2, FNavMeshPolyBase* NewPoly, ControlPointMap& CtrlPtMap)
{
	ControlPointList NewControlPointList;

	ControlPointList* PolyCtrlPts = CtrlPtMap.Find(OldPoly1);
	if(PolyCtrlPts != NULL)
	{
		for(INT CtIdx=0;CtIdx<PolyCtrlPts->Num();CtIdx++)
		{
			NewControlPointList.AddItem((*PolyCtrlPts)(CtIdx));
		}
		CtrlPtMap.Remove(OldPoly1);
	}


	PolyCtrlPts = CtrlPtMap.Find(OldPoly2);
	if(PolyCtrlPts != NULL)
	{
		for(INT CtIdx=0;CtIdx<PolyCtrlPts->Num();CtIdx++)
		{
			NewControlPointList.AddItem((*PolyCtrlPts)(CtIdx));
		}
		CtrlPtMap.Remove(OldPoly2);
	}


	CtrlPtMap.Set(NewPoly,NewControlPointList);

}

/**
 *	Uses an A* iterative search to find the highest area configuration of a rectangle expansion of the passed poly
 * (used for square merge)
 * @param Poly - the poly to expand
 * @param Mesh - the mesh that poly belongs to
 * @param bBailonCrappyNodes - if a node is realy lame, don't try to expand it (to make way for other nodes to expand)
 * @param NumMerges - out param of number of polys that got merged
 * @param CtrlPtMap - map of polys to control point lists
 * @return - area (score) of the result
 */
FLOAT ExpansionMinRectScore = 0.2;
#define NAVMESHGEN_MINRECTANGLE_SCORE ExpansionMinRectScore
#define MAX_CLOSED_LIST_SIZE 1000
FLOAT FindOptimalExpansionForRectanglePoly(FNavMeshPolyBase* Poly, UNavigationMeshBase* Mesh, UBOOL bBailOnCrappyNodes, INT& NumMerged, ControlPointMap& CtrlPtMap)
{
	RectConfigList OpenList;
	RectConfigList ClosedList;

	// mark starting poly as best config
	RectangleConfiguration BestConfig(Poly);

	// add starting config to open list
	OpenList.AddItem(BestConfig);

	while(OpenList.Num() > 0)
	{
		// pop the last node off of the open list 
		RectangleConfiguration CurConfig = *OpenList(OpenList.Num()-1);
		OpenList.RemoveItem(CurConfig);

		// if the new config is better than the current best, mark it as best
		if(CurConfig > BestConfig)
		{
			BestConfig = CurConfig;
		}

		// expand current config however possible
		ExpandRectConfigAllDirs(CurConfig,Mesh,OpenList,ClosedList,CtrlPtMap);
		ClosedList.AddItem(CurConfig);

		if(ClosedList.Num() > MAX_CLOSED_LIST_SIZE)
		{
			break;
		}	
	}

	FLOAT BestAspect = GetRectAspectRatio(BestConfig);


	// if the best we found was the same area as the start, don't bother with this
	// (this also allows other polys to cull this one if an opportunity arises)
	if(Poly->CalcArea() < FNavMeshPolyBase::CalcArea(BestConfig.Verts,Mesh))
	{

		FVector OldCtr = Poly->CalcCenter();
		Poly->ClearVerts();
		Poly->AddVerts(BestConfig.Verts);

		// remove absorbed polys
		TArray<FNavMeshPolyBase*> AbsorbedPolys;
		Mesh->GetIntersectingPolys(BestConfig.Verts,AbsorbedPolys);
		
		// find any polys which we missed that are inside the new poly, and remove them
		for(INT AbsorbedIdx=0;AbsorbedIdx<AbsorbedPolys.Num();AbsorbedIdx++)
		{
			FNavMeshPolyBase* CurPoly = AbsorbedPolys(AbsorbedIdx);
			if(CurPoly == Poly)
			{
				continue;
			}

			// DEBUG DEBUG draw a line from center of poly being absorbed to the new poly center
			//GWorld->GetWorldInfo()->DrawDebugLine(CurPoly->GetPolyCenter(),BestConfig.OrigPoly->GetPolyCenter()+FVector(0.f,0.f,10.f),255,0,0,TRUE);
			
			FVector Ctr(0.f),Extent(0.f);
			Poly->GetPolyBounds(LOCAL_SPACE).GetCenterAndExtents(Ctr,Extent);
			if(!FPolyAABBIntersect(Ctr,Extent,CurPoly))
			{
				continue;
			}

			
			CombineControlPoints(CurPoly,Poly,Poly,CtrlPtMap);

			++NumMerged;

			// DEBUG DEBUG draw a line from center of poly being absorbed to the new poly center
			//GWorld->GetWorldInfo()->DrawDebugLine(CurPoly->GetPolyCenter(),BestConfig.OrigPoly->GetPolyCenter(),255,255,255,TRUE);

			Mesh->RemovePoly(AbsorbedPolys(AbsorbedIdx));
		}
	}

	//DEBUG draw control points
// 	FVector Ctr = FNavMeshPolyBase::CalcCenter(BestConfig.Verts,Mesh);
// 	GWorld->GetWorldInfo()->DrawDebugLine(Ctr, Ctr+FVector(0,0,15),255,0,0,TRUE);
// 
// 	for(INT Idx=0;Idx<BestConfig.ControlPoints.Num();++Idx)
// 	{
// 		GWorld->GetWorldInfo()->DrawDebugLine(Mesh->GetVertLocation(BestConfig.ControlPoints(Idx)), Ctr,255,255,255,TRUE);
// 	}

	if(bBailOnCrappyNodes && BestAspect < NAVMESHGEN_MINRECTANGLE_SCORE)
	{
		return -1.0f;// indicate we didn't find one that was very good :D
	}

	return RectangleGoodnessHeuristic(BestConfig);
}

INT UNavigationMeshBase::MergeSquares()
{
	INT NumMerged = 0;
	FLOAT AccumScore = 0.f;

	ControlPointMap CtrlPtMap;

	INT Done=0;

	for(PolyList::TIterator Itt(BuildPolys.GetHead());Itt;)
	{
		FNavMeshPolyBase* CurPoly = *Itt;

		if(CurPoly->PolyVerts.Num()==4)
		{
			Done++;

			if( !CtrlPtMap.HasKey(CurPoly) )
			{
				ControlPointList NewList;
				AddPolyVertsAsControlPoints(CurPoly,NewList);
				CtrlPtMap.Set(CurPoly,NewList);
			}
	
			FLOAT ThisScore = FindOptimalExpansionForRectanglePoly(CurPoly,this,FALSE,NumMerged,CtrlPtMap);
			AccumScore+=ThisScore;
		}

		++Itt;

		static INT DebugCount = -1;
		if(DebugCount >0 && Done>=DebugCount)
		{
			break;
		}

	}

	debugf(TEXT("MERGESQUARES: TOTAL SCORE %.2f  REMOVED %i POLYS"),AccumScore,NumMerged);
	return NumMerged;
}

/**
 *	This will iterate over all polys merging adjacent polys that can be merged and remain convex in the process
 * @param AxisMask - mask to apply to verts (to control 2-d vs 3d checks etc)
 * @param bDoEdgeSmoothign - whether we should perform edge smoothing operations during the merge
 */
INT UNavigationMeshBase::MergePolys(FVector AxisMask, UBOOL bDoEdgeSmoothing)
{
	if( ! DO_SIMPLIFICATION || !ExpansionDoPolyMerge )
	{
		return 0;
	}

	ControlPointMap CtrlPtMap;

	// find shared edges, try to merge them
	static INT DebugCount = -1;
	INT NumMerged = 0;
	for(PolyList::TIterator Itt(BuildPolys.GetTail());Itt;)
	{
		if(DebugCount >= 0 && NumMerged >= DebugCount)
		{
			break;
		}
		FNavMeshPolyBase* Poly = *Itt;
		--Itt;

		if(!CtrlPtMap.HasKey(Poly))
		{
			ControlPointList NewList;
			AddPolyVertsAsControlPoints(Poly,NewList);
			CtrlPtMap.Set(Poly,NewList);
		}

		static TArray<FNavMeshPolyBase*> AdjacentPolys;
		AdjacentPolys.Reset();
		Poly->GetAdjacentPolys(AdjacentPolys);

		for(INT Idx=0;Idx<AdjacentPolys.Num();++Idx)
		{

			FNavMeshPolyBase* OtherPoly = AdjacentPolys(Idx);
			VERTID SharedVert1(0),SharedVert2(0);
			if(FindSharedEdge(Poly,OtherPoly,this,SharedVert1,SharedVert2))
			{
				if(!CtrlPtMap.HasKey(OtherPoly))
				{
					ControlPointList NewList;
					AddPolyVertsAsControlPoints(OtherPoly,NewList);
					CtrlPtMap.Set(OtherPoly,NewList);
				}

				FNavMeshPolyBase* NewPoly = TryCombinePolys(Poly,OtherPoly,SharedVert1,SharedVert2,FALSE,AxisMask,&CtrlPtMap,bDoEdgeSmoothing);
				if(NewPoly != NULL)
				{
					CombineControlPoints(Poly,OtherPoly,NewPoly,CtrlPtMap);
					NumMerged++;
					Itt = PolyList::TIterator(BuildPolys.GetTail());
					break;
				}
			}

		}
	}

	return NumMerged;
}

/**
 *	Counts the number of occurances of each vert in VertIDs and set the value in the output map
 * @param VertIDs - the array to count vertIDs from
 * @param out_NumOccurences - a map keyed on the vertID with a value of the number of occurances of that vertID in the array
 */
void CountOccurences(const TArray<VERTID>& VertIDs, TMap< VERTID, INT >& out_NumOccurences)
{
	out_NumOccurences.Empty(VertIDs.Num());
	// count number of occurances of each vertex in the poly
	for(INT PolyVertIdx=0;PolyVertIdx<VertIDs.Num();++PolyVertIdx)
	{
		VERTID Vertex = VertIDs(PolyVertIdx);
		INT* CurrentPtr = out_NumOccurences.Find(Vertex);
		INT Current = 0;
		if(CurrentPtr != NULL)
		{
			Current = *CurrentPtr;
		}

		out_NumOccurences.Set(Vertex, Current+1);
	}
}

/**
 *	used during internal split simplification.  Verify a given vertex ID's fitness to be removed.  
 *  this is done by finding all instances of the vertex in the poly and checking to see that the triplet
 *  (e.g. prev,cur,next) is the same for all instances.  This ensures that we only remove verts which will not 
 *  change the shape of the poly adversely
 * @param Mesh - owning mesh of the poly in question
 * @param Poly - poly we're checking
 * @param PrevVertID - vertex ID of previous vert of vert we're checking for removal
 * @param CurrVertID - vertex ID of the vert we're checking fitness for removal
 * @param NextVertID - vertex ID of th enext vert to the vert we're checking for removal
 */
UBOOL VerifyAdjacentVertsForAllInstances(UNavigationMeshBase* Mesh, FNavMeshPolyBase* Poly,VERTID PrevVertID,VERTID CurrVertID,VERTID NextVertID)
{
	for (INT PolyVertIdx=0;PolyVertIdx<Poly->PolyVerts.Num();++PolyVertIdx)
	{
		VERTID CurrLoopVertID = Poly->PolyVerts(PolyVertIdx);

		if(CurrLoopVertID == CurrVertID)
		{
			INT PrevIdx = (PolyVertIdx==0) ? Poly->PolyVerts.Num()-1 : PolyVertIdx-1;
			INT NextIdx = (PolyVertIdx+1) % Poly->PolyVerts.Num();

			VERTID PrevLoopVertID = Poly->PolyVerts(PrevIdx);
			VERTID NextLoopVertID = Poly->PolyVerts(NextIdx);

			if (  !(PrevVertID == PrevLoopVertID && NextVertID == NextLoopVertID) 
			   && !(PrevVertID == NextLoopVertID && NextVertID == PrevLoopVertID))
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/**
 * Verifies that removing the passed vertex is not going to cause a narrow angle that comes back in on itself and creates problems for simplification
 * @param Mesh - owning mesh of the poly in question
 * @param Poly - poly we're checking
 * @param RemovalVertID - vertex ID that is about to be removed
 * @return TRUE if it's ok to remove the passed vertex
 */
UBOOL VerifyCornerAngleForAllInstances(UNavigationMeshBase* Mesh, FNavMeshPolyBase* Poly,VERTID RemovalVertID)
{
	static FLOAT MaxCornerAngle = 0.9f;
#define MAX_CORNER_ANGLE MaxCornerAngle
	for (INT PolyVertIdx=0;PolyVertIdx<Poly->PolyVerts.Num();++PolyVertIdx)
	{
		VERTID CurrLoopVertID = Poly->PolyVerts(PolyVertIdx);

		if(CurrLoopVertID == RemovalVertID)
		{
			// find corner after removing the vert
			INT PrevIdx = PolyVertIdx-2;
			if( PrevIdx < 0 )
			{
				PrevIdx += Poly->PolyVerts.Num();
			}

			INT CurrentIdx = /*(PolyVertIdx+1) % Poly->PolyVerts.Num();*/(PolyVertIdx==0) ? Poly->PolyVerts.Num()-1 : PolyVertIdx-1;;
			INT NextIdx = (PolyVertIdx+1) % Poly->PolyVerts.Num();

			const FVector PrevLoopVertLoc = Mesh->GetVertLocation(Poly->PolyVerts(PrevIdx),LOCAL_SPACE);
			const FVector CurrentLoopVertLoc = Mesh->GetVertLocation(Poly->PolyVerts(CurrentIdx),LOCAL_SPACE);
			const FVector NextLoopVertLoc = Mesh->GetVertLocation(Poly->PolyVerts(NextIdx),LOCAL_SPACE);

			FLOAT Dot = ((PrevLoopVertLoc-CurrentLoopVertLoc).SafeNormal() | (NextLoopVertLoc-CurrentLoopVertLoc).SafeNormal());
			if( Dot > MAX_CORNER_ANGLE )
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/**
 *	tries to remove unnecessary verts from the given poly
 * @param Poly - the poly to try and remove unnecessary verts from
 * @return - number of vertexes removed
 */
INT UNavigationMeshBase::SimplifyEdgesOfPoly(FNavMeshPolyBase* Poly)
{
	TLookupMap<VERTID> VertsOnAnEdge_EndPointsExcluded;
	TLookupMap<VERTID> VertsOnAnEdge_EndPointsIncluded;
	TLookupMap<VERTID> VertsOnMiddleOfEdge_IncludingSelf;
	TLookupMap<VERTID> VertsWhoseEdgesHaveVertsOnThem;
	TArray<FNavMeshPolyBase*> ExcludedPolys,Blank;
	ExcludedPolys.AddItem(Poly);

	for(INT VertIdx=0;VertIdx<Poly->PolyVerts.Num();++VertIdx)
	{
		const VERTID ThisVert = Poly->PolyVerts(VertIdx);
		if(IsVertexOnEdge(ThisVert ,this,ExcludedPolys,FALSE))
		{
			VertsOnAnEdge_EndPointsIncluded.AddItem(ThisVert );
		}
		if(IsVertexOnEdge(ThisVert ,this,ExcludedPolys,TRUE))
		{
			VertsOnAnEdge_EndPointsExcluded.AddItem(ThisVert );
		}
		if(IsVertexOnEdge(ThisVert ,this,Blank,TRUE))
		{
			VertsOnMiddleOfEdge_IncludingSelf.AddItem(ThisVert );
		}
		if( DoesVertexHaveEdgesWithOtherVerts(Poly,VertIdx) )
		{
			VertsWhoseEdgesHaveVertsOnThem.AddItem( ThisVert );
		}

	}
	
	INT NumRemoved=-1;
	INT TotalRemoved = 0;
	TArray<VERTID> OrigPoly=Poly->PolyVerts;

	TMap< VERTID,INT> NumOccurances;
	CountOccurences(Poly->PolyVerts,NumOccurances);

	const FVector PolyNorm = Poly->GetPolyNormal(LOCAL_SPACE);

	while( NumRemoved == -1 || NumRemoved > 0 )
	{
		NumRemoved = 0;
		for(INT VertIdx=Poly->PolyVerts.Num()-1;VertIdx>=0;--VertIdx)
		{		
			if( NumOccurances.FindRef( Poly->PolyVerts(VertIdx) ) > 1 )
			{
				continue;
			}


			FLOAT ConvexTol = NAVMESHGEN_EDGESMOOTHING_CONVEX_TOLERANCE;
			if( VertsOnMiddleOfEdge_IncludingSelf.HasKey(Poly->PolyVerts(VertIdx)) )
			{
				ConvexTol = 0.25f;
			}

			static UBOOL bDoOtherEdgeVerification = TRUE;
			if( bDoOtherEdgeVerification )
			{
				if( VertsWhoseEdgesHaveVertsOnThem.HasKey(Poly->PolyVerts(VertIdx)) )
				{
					continue;
				}
			}

			// if this comes back on itself, remove the vertex and it's prev
			INT NextIdx = (VertIdx+1) % Poly->PolyVerts.Num();
			VERTID NextVert = Poly->PolyVerts(NextIdx);
			VERTID PrevVert = (VertIdx-1<0) ? Poly->PolyVerts(Poly->PolyVerts.Num()-1) : Poly->PolyVerts(VertIdx-1); 

			if( NextVert == PrevVert && Poly->PolyVerts.Num() > 2 )
			{
				// remove current and next
				NumRemoved+=2;
				OrigPoly.RemoveItem(Poly->PolyVerts(VertIdx));
				Poly->RemoveVertexAtLocalIdx(VertIdx);
				
				if( NextIdx != 0 )
				{
					Poly->RemoveVertexAtLocalIdx(VertIdx);
				}
				else
				{
					Poly->RemoveVertexAtLocalIdx(0);
				}

				VertIdx=Poly->PolyVerts.Num()-1;
				CountOccurences(Poly->PolyVerts,NumOccurances);
				checkSlow(!Poly->PolyVerts.ContainsItem(65535));
				continue;
			}


			if(PerformEdgeSmoothingStep(Poly->PolyVerts,ExcludedPolys,VertsOnAnEdge_EndPointsIncluded,VertsOnAnEdge_EndPointsExcluded,VertIdx,ConvexTol,PolyNorm,FALSE))
			{				
				NumRemoved++;
				Poly->RemoveVertexAtLocalIdx(VertIdx);
				VertIdx=Poly->PolyVerts.Num()-1;
				CountOccurences(Poly->PolyVerts,NumOccurances);
				checkSlow(!Poly->PolyVerts.ContainsItem(65535));
			}

		}

		TotalRemoved += NumRemoved;
	}

	// second pass to ensure we're within tolerance of all the original verts
	static UBOOL bDoSecondPass = TRUE;
	UBOOL bAdded=FALSE;

	if( bDoSecondPass && Poly->PolyVerts.Num() > 0 )
	{
		INT Count=0;
		INT NewPolyIdx=-1;
		for(INT OrigVertIdx=0;Count<OrigPoly.Num()*2;OrigVertIdx = (OrigVertIdx+1)%OrigPoly.Num(),++Count)
		{
			INT PerhapsNewNewPolyIdx = (NewPolyIdx+1)%Poly->PolyVerts.Num();
			if( OrigPoly(OrigVertIdx) == Poly->PolyVerts(PerhapsNewNewPolyIdx) )
			{
				// if we just found the first shared vert between old/new reset the count
				if(NewPolyIdx < 0)
				{
					Count=0;
				}
				NewPolyIdx = PerhapsNewNewPolyIdx;
			}

			if(NewPolyIdx < 0)
			{
				continue;
			}

			VERTID Current_Simplified_Idx = Poly->PolyVerts(NewPolyIdx);
			INT NextPolyIdx_Local = (NewPolyIdx+1) % Poly->PolyVerts.Num();
			VERTID Next_Simplified_Idx = Poly->PolyVerts( NextPolyIdx_Local );

			const FVector Simpl_Loc = GetVertLocation(Current_Simplified_Idx,LOCAL_SPACE);
			const FVector Next_Simpl_Loc = GetVertLocation(Next_Simplified_Idx,LOCAL_SPACE);
			const FVector Orig_Loc = GetVertLocation(OrigPoly(OrigVertIdx),LOCAL_SPACE);
			FVector Closest(0.f);
			if(PointDistToSegment(Orig_Loc,Simpl_Loc,Next_Simpl_Loc,Closest) > NAVMESHGEN_EDGESMOOTHING_CONVEX_TOLERANCE)
			{
				Poly->PolyVerts.InsertItem(OrigPoly(OrigVertIdx),NextPolyIdx_Local);
				Poly->NavMesh->Verts(OrigPoly(OrigVertIdx)).ContainingPolys.AddUniqueItem(Poly);
				NewPolyIdx=NextPolyIdx_Local;
			}
		}
	}

	
	return OrigPoly.Num() - Poly->PolyVerts.Num();
}

// forward declare
UBOOL DoesSplitIntersectExistingEdge(UNavigationMeshBase* Mesh, VERTID Vert0, VERTID Vert1, const TArray<VERTID>& ConcaveShape, UBOOL bCheckEdgeProximity);

/**
 *	SimplifyInternalSplitsForPoly
 *  - given a concave poly which as splits leading to holes in the poly, this will remove unnecessary verts in these splits
 *    to smooth them out e.g.
 *    ----------------
 *    |    ____      |
 *	  |   |    |	 |
 *	  |   |____|_	 |
 * 	  |          |_	 |	
 *	  |            |_|
 *	  |______________|
 *
 *     Becomes: 
 *    ----------------
 *    |    ____      |
 *	  |   |    |	 |
 *	  |   |____|	 |
 * 	  |         \	 |	
 *	  |          \   |
 *	  |___________\__|
 *
 * @param Mesh - the owning mesh of the poly we're simplifying
 * @param Poly - the poly we're simplifying
 */
INT SimplifyInternalSplitsForPoly(UNavigationMeshBase* Mesh, FNavMeshPolyBase* Poly)
{

	TMap< VERTID, INT > NumOccurences;
	CountOccurences(Poly->PolyVerts,NumOccurences);

	INT NumRemoved=0;
	for(INT VertIdx=Poly->PolyVerts.Num()-1;VertIdx>=0;--VertIdx)
	{
		INT PrevIdx = (VertIdx==0) ? Poly->PolyVerts.Num()-1 : VertIdx-1;
		INT NextIdx = (VertIdx+1) % Poly->PolyVerts.Num();

		VERTID PrevVertID = Poly->PolyVerts(PrevIdx);
		VERTID NextVertID = Poly->PolyVerts(NextIdx);
		VERTID CurrVertID = Poly->PolyVerts(VertIdx);

		TArray<FNavMeshPolyBase*> Excluded;
		Excluded.AddItem(Poly);

		UBOOL bCountValid = ( NumOccurences.FindRef(PrevVertID) >= 2 
			&& NumOccurences.FindRef(NextVertID) >= 2 
			&& NumOccurences.FindRef(CurrVertID) >= 2);

		if (bCountValid && !IsVertexOnEdge(CurrVertID,Mesh,Excluded,FALSE))
		{

			// check to make sure that all instances of this vertex in the poly share the same adjacent verts
			if( VerifyAdjacentVertsForAllInstances(Mesh,Poly,PrevVertID,CurrVertID,NextVertID) &&
				VerifyCornerAngleForAllInstances(Mesh,Poly,CurrVertID)
			  )
			{

				// ok we have a good candidate, see if removing the center vert would intersect with other edges
				if( ! DoesSplitIntersectExistingEdge(Mesh,PrevVertID,NextVertID,Poly->PolyVerts,TRUE) )
				{
					// remove both instances of the vertex
					Poly->RemoveVertex(CurrVertID);
					NumRemoved++;
					VertIdx=Poly->PolyVerts.Num()-1;
					CountOccurences(Poly->PolyVerts,NumOccurences);
				}
			}
		}
	}

	return NumRemoved;
}

/**
 *	this will test if OffLoc is within the angle formed by the three passed vectors.  
 * @param OffLoc - the location you want to know whether is inside the angle or not
 * @param Prev - first point of corner (Previous on a clockwise wound poly)
 * @param Current - center point of corner
 * @param Next - last point of corner (Next on a clockwise wound poly)
 */
UBOOL IsWithinEdgeAngle(const FVector& OffLoc, const FVector& Prev, const FVector& Current, const FVector& Next)
{	
	const FVector PrevToCurrent = (Prev-Current).SafeNormal();
	const FVector NextToCurrent = (Next-Current).SafeNormal();
	const FVector OffToCurrent = (OffLoc-Current).SafeNormal();

	FVector Compare_Dir = (PrevToCurrent+NextToCurrent)/2.0f;
	FLOAT LegDot = (PrevToCurrent|NextToCurrent);
	UBOOL bInvert=FALSE;
	if( appIsNearlyEqual(LegDot,-1.0f) )
	{
		Compare_Dir = (PrevToCurrent^FVector(0.f,0.f,1.f)).SafeNormal();
	}
	else if( (NextToCurrent^PrevToCurrent).Z <= 0.f )
	{
		bInvert=TRUE;
	}
		
	FLOAT CompareDot = PrevToCurrent|Compare_Dir;
	FLOAT TestDot = OffToCurrent | Compare_Dir;
	if(bInvert)
	{
		return (TestDot < CompareDot);
	}
	else 
	{
		return (TestDot >= CompareDot);
	}	
}

/**
 *	this will split a poly into as close to equal pieces as possible recursively until all polys are below the max poly count for a slab
 * @param Poly - the poly to split
 * @return - FALSE if unsuccesful
 */
void SplitPolyAtLocalVertIndexes( const TArray<VERTID>& Poly, INT SplitVertAIdx, INT SplitVertBIdx, TArray< VERTID >& out_NewPolyA, TArray< VERTID >& out_NewPolyB);
UBOOL VerifyWinding(const TArray<VERTID>& Poly, UNavigationMeshBase* Mesh);
UBOOL LimitSizeOfPoly(FNavMeshPolyBase* Poly)
{

	INT BestDelta = -1;
	TArray<VERTID> BestPolyA,BestPolyB;
	INT Vert0(0),Vert1(0);
	FVector PrevOuter(0.f),NextOuter(0.f),Prevd(0.f),Nextd(0.f);
	UNavigationMeshBase* Mesh = Poly->NavMesh;
	

	for(INT OuterVertIdx=0;OuterVertIdx<Poly->PolyVerts.Num();++OuterVertIdx)	
	{
		const VERTID OuterVertID = Poly->PolyVerts(OuterVertIdx);

		INT Outer_NextID = Poly->PolyVerts((OuterVertIdx+1)%Poly->PolyVerts.Num());
		INT Outer_PrevID = Poly->PolyVerts((OuterVertIdx==0) ? Poly->PolyVerts.Num()-1 : OuterVertIdx-1);

		const FVector Outer_Prev = Mesh->GetVertLocation(Outer_PrevID,LOCAL_SPACE);
		const FVector Outer_Next = Mesh->GetVertLocation(Outer_NextID,LOCAL_SPACE);
		const FVector Outer_Current = Mesh->GetVertLocation(OuterVertID,LOCAL_SPACE);


		for (INT InnerVertIdx=0;InnerVertIdx<Poly->PolyVerts.Num();++InnerVertIdx)
		{
		
			// don't try splitting to ourselves :) 
			if(InnerVertIdx == OuterVertIdx)
			{
				continue;
			}

			const VERTID InnerVertID = Poly->PolyVerts(InnerVertIdx);

			// don't try splitting to direct neighbors
			INT NextID = (InnerVertIdx+1)%Poly->PolyVerts.Num();
			INT PrevID = (InnerVertIdx==0) ? Poly->PolyVerts.Num()-1 : InnerVertIdx-1;
			if( NextID == OuterVertIdx || PrevID == InnerVertIdx)
			{
				continue;
			}

			const FVector Prev = Mesh->GetVertLocation(Poly->PolyVerts(PrevID),LOCAL_SPACE);
			const FVector Next = Mesh->GetVertLocation(Poly->PolyVerts(NextID),LOCAL_SPACE);
			const FVector Current = Mesh->GetVertLocation(InnerVertID,LOCAL_SPACE);
			
			if(!IsWithinEdgeAngle(Outer_Current,Prev,Current,Next) || 
			   !IsWithinEdgeAngle(Current,Outer_Prev,Outer_Current,Outer_Next))
			{
				continue;
			}
			
			// if this split intersects existing edges of the poly.. skip it
			if(DoesSplitIntersectExistingEdge(Poly->NavMesh,OuterVertID,InnerVertID,Poly->PolyVerts,TRUE))
			{
				continue;
			}

			// split the poly into two vert arrays
			TArray<VERTID> PolyA,PolyB;
			SplitPolyAtLocalVertIndexes(Poly->PolyVerts,OuterVertIdx,InnerVertIdx,PolyA,PolyB);
	

			// if the winding on either new poly is flipped it means the split crossed the boundary of the poly .. bad things
			if( ! VerifyWinding(PolyA,Mesh) || ! VerifyWinding(PolyB,Mesh) )
			{
				continue;
			}	

			// keep track to the closest to even split we've seen
			INT ThisDelta = Abs<INT>(PolyA.Num() - PolyB.Num());
			if( BestDelta < 0 || BestDelta > ThisDelta)
			{
				BestDelta = ThisDelta;
				BestPolyA = PolyA;
				BestPolyB = PolyB;
				Vert0=OuterVertIdx;
				Vert1=InnerVertIdx;
				PrevOuter = Outer_Prev;
				NextOuter = Outer_Next;
				Prevd = Prev;
				Nextd = Next;
			}
		}
	}	

	if( BestDelta > -1 )
	{

		FLOAT PolyHeight = Poly->GetPolyHeight();
		Mesh->RemovePoly(Poly);
		
		FNavMeshPolyBase* NewPolyA = Mesh->AddPolyFromVertIndices(BestPolyA,PolyHeight);
		FNavMeshPolyBase* NewPolyB = Mesh->AddPolyFromVertIndices(BestPolyB,PolyHeight);


		if(BestPolyA.Num() > NAVMESHGEN_MAX_SLAB_VERT_COUNT)
		{
			if(!LimitSizeOfPoly(NewPolyA))
			{
				return FALSE;
			}
		}

		if(BestPolyB.Num() > NAVMESHGEN_MAX_SLAB_VERT_COUNT)
		{
			if(!LimitSizeOfPoly(NewPolyB))
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/**
 *	This will merge the mesh into large concave polygons, split up only by max size and extreme slope changes.  These concave 'slabs' will then be decomposed into convex shapes
 * @return - the number of polys removed during merging
 */
typedef FNavMeshPolyBase* PolyPtr;
static FNavMeshPolyBase* GPoly = NULL;

IMPLEMENT_COMPARE_CONSTREF( PolyPtr, SmallVolumeFirst, \
{ FLOAT BVol = (B->GetPolyBounds() + GPoly->GetPolyBounds()).GetVolume(); \
  FLOAT AVol = (A->GetPolyBounds() + GPoly->GetPolyBounds()).GetVolume(); \
  if( appIsNearlyEqual(BVol,AVol) )\
  {\
	  return A->Item > B->Item;\
  }\
  return (AVol - BVol) > 0.f ? 1 : -1; } )

INT UNavigationMeshBase::MergePolysConcave()
{
	if( ! DO_SIMPLIFICATION || !ExpansionDoPolyConcaveMerge)
	{
		return 0;
	}


	static INT DebugMergeCount = -1;
	// find shared edges, try to merge them
	INT NumMerged = 0;
	for(PolyList::TIterator Itt(BuildPolys.GetTail());Itt;)
	{
		if(NumMerged >= DebugMergeCount && DebugMergeCount>=0)
		{
			break;
		}

		GPoly = *Itt;
		--Itt;

		FVector Ctr(0.f),Extent(0.f);
		FBox Bounds = GPoly->GetPolyBounds(LOCAL_SPACE);
		FLOAT Diag = Bounds.GetExtent().Size();
		Bounds.ExpandBy(Diag).GetCenterAndExtents(Ctr,Extent);
		TArray<FNavMeshPolyBase*> Polys;
		GetAllPolysNearPoint(Ctr,Extent,Polys);

		// MT-> this will not work if we multi-thread this.. need to sort some other way (poly ref has to be static because comparator is static)
		// sort based on polys which when added will result in the smallest possible bounding box
		Sort<USE_COMPARE_CONSTREF(PolyPtr,SmallVolumeFirst)>(&Polys(0),Polys.Num());

		UBOOL bMerged=FALSE;
		FNavMeshPolyBase* StartingPoly = GPoly;
		for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
		{			
			FNavMeshPolyBase* AdjacentPoly = Polys(PolyIdx);
			if(AdjacentPoly == StartingPoly)
			{
				continue;
			}


			FNavMeshPolyBase* NewPoly = TryCombinePolysConcave(GPoly,AdjacentPoly);
			if(NewPoly != NULL)
			{
				NumMerged++;
				bMerged=TRUE;
				GPoly = NewPoly;
				if(NumMerged >= DebugMergeCount && DebugMergeCount>=0)
				{
					break;
				}
			}
		}

		if(bMerged)
		{
			Itt = PolyList::TIterator(BuildPolys.GetTail());
		}
	}


	AlignAdjacentPolys();

	if( !ExpansionDoConcaveSlabsOnly || ExpansionDoEdgeSimplificationEvenInConcaveSlabMode)
	{
		SimplifyEdgesOfMesh();
	}

	// simplify connective boundaries with holes
	static UBOOL bDoInternalSimplification=TRUE;
	if( bDoInternalSimplification )
	{
		INT TotalRemoved=0;
		for(PolyList::TIterator Itt(BuildPolys.GetTail());Itt;--Itt)
		{
			TotalRemoved += SimplifyInternalSplitsForPoly(this,*Itt);	
		}
	}

	
	// if we have a max vert count set, split up the biggies 
	if( NAVMESHGEN_MAX_SLAB_VERT_COUNT > -1 )
	{
		for(PolyList::TIterator Itt(BuildPolys.GetTail());Itt;)
		{
			FNavMeshPolyBase* Poly = *Itt;
			--Itt;
			if(Poly->PolyVerts.Num() > NAVMESHGEN_MAX_SLAB_VERT_COUNT )
			{
				LimitSizeOfPoly(Poly);
			}
		}
	}


	SnapCloseInternalConcaveVerts();

	return NumMerged;
}

/**
 * will return whether or not the poly represented by the passed list of verids is convex
 * @param VertList - list of vertex IDs that represent the polygon to be tested
 * @param Tolerace - the tolerance with which to test for convexity
 * @param PolyNormal - (optional) normal of poly (if already computed.. to save cycles)
 * @return TRUE if the polygon is convex 
 */
extern INT Increment(INT OldIdx,INT Direction, INT NumElements);
static INT DEBUGDRAWS = 50;
UBOOL UNavigationMeshBase::IsConvex(const TArray<VERTID>& VertList, FLOAT Tolerance/*=-1.f*/, FVector PolyNorm)
{
	if(Tolerance < 0.f)
	{
		Tolerance = NAVMESHGEN_CONVEX_TOLERANCE;
	}
	else if(appIsNearlyEqual(Tolerance,0.f))
	{
		Tolerance = THRESH_POINT_ON_PLANE;
	}

	//checkSlowish(VertList.Num() >= 3);
	if(VertList.Num() < 3)
	{
		debugf(TEXT("WARNING! UNavigationMeshBase::IsConvex called on poly with less than 3 vertices."));
		return FALSE;
	}

	if( PolyNorm.IsNearlyZero() )
	{
		PolyNorm = FNavMeshPolyBase::CalcNormal(VertList,this,LOCAL_SPACE);
	}

	// construct plane for each edge and check other verts to make sure they're inside that plane
	for(INT VertIdx=0;VertIdx<VertList.Num();++VertIdx)
	{
		const FVector This_Vert = Verts(VertList(VertIdx));
		const FVector Next_Vert = Verts(VertList(Increment(VertIdx,1,VertList.Num())));
		
		const FVector Plane_Normal = ((This_Vert-Next_Vert).SafeNormal()^PolyNorm).SafeNormal();
		const FPlane This_Plane = FPlane(This_Vert,Plane_Normal);

		for(INT InnerVertIdx=0;InnerVertIdx<VertList.Num();++InnerVertIdx)
		{
			const FVector Inner_This_Vert = Verts(VertList(InnerVertIdx));

			const FLOAT DistFromPlane = This_Plane.PlaneDot(Inner_This_Vert);

			if(DistFromPlane < -Tolerance)
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/**
 * <<static version that takes actual locations>>
 * will return whether or not the poly represented by the passed list of verids is convex
 * @param VertList - list of vertex locations that represent the polygon to be tested
 * @param Tolerace - the tolerance with which to test for convexity
 * @param PolyNormal - normal of poly (if already computed.. to save cycles)
 * @return TRUE if the polygon is convex 
 */
UBOOL UNavigationMeshBase::IsConvex(const TArray<FVector>& VertList, FLOAT Tolerance/*=-1.f*/, FVector PolyNorm/*=FVector(0.f)*/)
{
	if(Tolerance < 0.f)
	{
		Tolerance = NAVMESHGEN_CONVEX_TOLERANCE;
	}
	else if(appIsNearlyEqual(Tolerance,0.f))
	{
		Tolerance = THRESH_POINT_ON_PLANE;
	}

	//checkSlowish(VertList.Num() >= 3);
	if(VertList.Num() < 3)
	{
		debugf(TEXT("WARNING! UNavigationMeshBase::IsConvex called on poly with less than 3 vertices."));
		return FALSE;
	}

	if( PolyNorm.IsNearlyZero() )
	{
		PolyNorm = FNavMeshPolyBase::CalcNormal(VertList);
	}

	// construct plane for each edge and check other verts to make sure they're inside that plane
	for(INT VertIdx=0;VertIdx<VertList.Num();++VertIdx)
	{
		const FVector This_Vert = VertList(VertIdx);
		const FVector Next_Vert = VertList(Increment(VertIdx,1,VertList.Num()));

		const FVector Plane_Normal = ((This_Vert-Next_Vert).SafeNormal()^PolyNorm).SafeNormal();
		const FPlane This_Plane = FPlane(This_Vert,Plane_Normal);

		for(INT InnerVertIdx=0;InnerVertIdx<VertList.Num();++InnerVertIdx)
		{
			const FVector Inner_This_Vert = VertList(InnerVertIdx);

			const FLOAT DistFromPlane = This_Plane.PlaneDot(Inner_This_Vert);

			if(DistFromPlane < -Tolerance)
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/**
 * will remove "silly" polygons from the mesh (e.g. polys which are less than a certain area)
 * @return - the number of polys removed 
 */
// sillyness is not allowed
INT UNavigationMeshBase::CullSillyPolys()
{
	INT Ret = 0;
	for(PolyList::TIterator Itt(BuildPolys.GetTail());Itt;)
	{
		FNavMeshPolyBase* CurPoly = *Itt;
		--Itt;

		UBOOL bRemove = (CurPoly->PolyVerts.Num() < 3 
						|| CurPoly->CalcArea() < NAVMESHGEN_MIN_POLY_AREA);

		if(bRemove)
		{			
			Ret++;
			RemovePoly(CurPoly);
			continue;
		}
	}

	debugf(TEXT("CullSillyPolys removed %i polys."),Ret);
	return Ret;
}


/**
 * will generate a list of vertIDs that are shared between both polys
 *
 * @param Poly1 - poly1 to check for shared verts
 * @param Poly2 - poly2 to check for shared verts
 * @param out_SharedVerts - out array of shared verts
 */
void FindSharedVerts(FNavMeshPolyBase* Poly1, FNavMeshPolyBase* Poly2, TArray<VERTID>& out_SharedVerts)
{
	for(INT Poly0VertIDX=0;Poly0VertIDX<Poly1->PolyVerts.Num(); ++Poly0VertIDX)
	{
		VERTID Vert = Poly1->PolyVerts(Poly0VertIDX);
		if( Poly2->PolyVerts.ContainsItem(Vert) )
		{
			out_SharedVerts.AddItem(Vert);
		}
	}
}


/**
 * used by concave poly merging (TryCombinePolysConcave)
 * attempts to find an overlapping segment between the two polys (e.g. where we'd normally create an edge)
 * also checks to see that if there are multiple edge possibilities that they are connected to each other so we don't
 * merge a hole into the slab
 * @param PolyA - first poly to check with
 * @param POlyB - second poly to check with
 * @param out_ASharing_0 - vertexID of the first vertex in the edge in Poly A which is adjacent to an edge in poly B
 * @param out_ASharing_1 - vertexID of the second vertex in the edge in Poly A which is adjacent to an edge in poly B
 * @param out_BSharing_0 - vertexID of the first vertex in the edge in Poly B which is adjacent to an edge in poly A
 * @param out_BSharing_1 - vertexID of the second vertex in the edge in Poly B which is adjacent to an edge in poly A
 * @param out_EdgePt0    - the first point of the new edge/overlapping segment
 * @param out_EdgePt1    - second point of the new edge/overlapping segment
 * @return - TRUE If a valid adjacent edge was found
 */
UBOOL FindAdjacentEdgeBetweenPolyAandPolyB(
										FNavMeshPolyBase* PolyA,
										FNavMeshPolyBase* PolyB,
										VERTID& out_ASharing_0,
										VERTID& out_ASharing_1,
										VERTID& out_BSharing_0,
										VERTID& out_BSharing_1,
										FVector& out_EdgePt_0,
										FVector& out_EdgePt_1
										)
{

	// list of overlapping 'edge' segments that are candidates for merging
	TArray<UNavigationMeshBase::FEdgeTuple> Edge_Segments;
	TArray<FNavMeshEdgeBase> TempEdges;
	const FBox PolyABox = PolyA->GetPolyBounds(WORLD_SPACE).ExpandBy(5.f);
	const FBox PolyBBox = PolyB->GetPolyBounds(WORLD_SPACE).ExpandBy(5.f);

	PolyA->NavMesh->AddTempEdgesForPoly(*PolyA,TempEdges,&PolyBBox);
	PolyB->NavMesh->AddTempEdgesForPoly(*PolyB,TempEdges,&PolyABox);

	PolyA->NavMesh->CreateEdgeConnectionsInternal(TempEdges,FALSE,FALSE,FALSE,&Edge_Segments,0.1f,GetStepSize(NAVMESHGEN_MAX_SUBDIVISIONS));

	// cull duplicates (normally we will have 2x.. one for each direction)
	for(INT OuterEdgeIdx=Edge_Segments.Num()-1;OuterEdgeIdx>=0;--OuterEdgeIdx)
	{
		for(INT EdgeIdx=0;EdgeIdx<Edge_Segments.Num();++EdgeIdx)
		{
			if(EdgeIdx == OuterEdgeIdx)
			{
				continue;
			}

			if( 
				( PointsEqualEnough(Edge_Segments(EdgeIdx).Pt0,Edge_Segments(OuterEdgeIdx).Pt0,0.01f) ||
				  PointsEqualEnough(Edge_Segments(EdgeIdx).Pt0,Edge_Segments(OuterEdgeIdx).Pt1,0.01f))
				&& 				
				( PointsEqualEnough(Edge_Segments(EdgeIdx).Pt1,Edge_Segments(OuterEdgeIdx).Pt0,0.01f) || 
				  PointsEqualEnough(Edge_Segments(EdgeIdx).Pt1,Edge_Segments(OuterEdgeIdx).Pt1,0.01f))
			  )
			{
				Edge_Segments.Remove(EdgeIdx);
				break;
			}
		}
	}



	if(Edge_Segments.Num() == 0)
	{
		return FALSE;
	}


	//DEBUG DEBUG DEBUG
// 	for(INT OuterEdgeIdx=0;OuterEdgeIdx<Edge_Segments.Num();++OuterEdgeIdx)
// 	{
// 		GWorld->GetWorldInfo()->DrawDebugLine(Edge_Segments(OuterEdgeIdx).Pt0+FVector(0,0,5),Edge_Segments(OuterEdgeIdx).Pt1+FVector(0,0,5),255,255,0,TRUE);
// 	}
		
	// ok! so we have a valid shared edge situation.. now we need to set the out values using the first segment found 
	out_EdgePt_0=Edge_Segments(0).Pt0;
	out_EdgePt_1=Edge_Segments(0).Pt1;

	if(PolyA == Edge_Segments(0).Poly0)
	{
		out_ASharing_0 = Edge_Segments(0).Poly0_Vert0;
		out_ASharing_1 = Edge_Segments(0).Poly0_Vert1;
		
		out_BSharing_0 = Edge_Segments(0).Poly1_Vert0;
		out_BSharing_1 = Edge_Segments(0).Poly1_Vert1;
	}
	else
	{
		check(PolyB == Edge_Segments(0).Poly0);
		out_BSharing_0 = Edge_Segments(0).Poly0_Vert0;
		out_BSharing_1 = Edge_Segments(0).Poly0_Vert1;

		out_ASharing_0 = Edge_Segments(0).Poly1_Vert0;
		out_ASharing_1 = Edge_Segments(0).Poly1_Vert1;
	}

	// DEBUG DEBUG DEBUG
//	GWorld->GetWorldInfo()->DrawDebugSphere(out_EdgePt_0,15,12,0,0,255,TRUE);
//	GWorld->GetWorldInfo()->DrawDebugSphere(out_EdgePt_1,15,12,0,0,255,TRUE);
// 	GWorld->GetWorldInfo()->DrawDebugSphere(out_ASharing_0,5,12,0,255,0,TRUE);
// 	GWorld->GetWorldInfo()->DrawDebugSphere(out_ASharing_1,5,12,0,255,0,TRUE);
// 	GWorld->GetWorldInfo()->DrawDebugSphere(out_BSharing_0,5,12,255,255,0,TRUE);
// 	GWorld->GetWorldInfo()->DrawDebugSphere(out_BSharing_1,5,12,255,255,0,TRUE);

	return TRUE;
}


/**
 * Used during concave poly merging (TryCombinePolysConcave).  Determines the vertx in the passed poly to start adding to the combined poly
 * Choose StartVert:
 *	Start with either SharingEdgeVert_0 or SharingEdgeVert_1 
 *	whichever's next is not one of the EdgePoints
 *  if both verts have nexts which are not edgepoints, choose the one which is within the edge segment
 * @param Poly - poly to choose starting vert for
 * @param EdgePt0 - location of found edgepoint_0
 * @param EdgePt1 - location of found edgepoint_1
 * @param SharingEdgeVert_0 - vert on the edge of the poly that is adjacent to the other poly (edge which was used to find EdgePts)
 * @param SharingEdgeVert_1 - other vert on the edge of the poly that is adjacent to the other poly (edge which was used to find EdgePts)
 * @return - the local index (local to Poly) of the vert we should start adding from
 */
FLOAT ExpansionNearEdgePtTolerance = 1.0f;
#define NEAR_EDGE_PT_TOL ExpansionNearEdgePtTolerance
INT UNavigationMeshBase::FindStartingIndex( FNavMeshPolyBase* Poly, FVector EdgePt0, FVector EdgePt1, VERTID SharingEdgeVert_0, VERTID SharingEdgeVert_1 )
{
	// find local indexes of verts in poly
	INT LocalIdx_0 = 0;
	INT LocalIdx_1 = 0;

	// find the spot in the poly verts array that corresponds to both sharing indexes being next to each other
	UBOOL bFound=FALSE;
	for(INT Idx=0;Idx<Poly->PolyVerts.Num();++Idx)
	{
		LocalIdx_0=Idx;
		VERTID CurrentVertID = Poly->PolyVerts(Idx);
		LocalIdx_1 = (Idx+1) % Poly->PolyVerts.Num();
		VERTID NextVertID = Poly->PolyVerts( LocalIdx_1 );

		if(CurrentVertID == SharingEdgeVert_0 && NextVertID == SharingEdgeVert_1)
		{
			bFound=TRUE;
			break;
		}
	}

	check(bFound);

	// find local indexes of next verts for 0, 1
	INT LocalNextIdx_0 = (LocalIdx_0+1) % Poly->PolyVerts.Num();
	INT NextVert_0 = Poly->PolyVerts(LocalNextIdx_0);
	INT LocalNextIdx_1 = (LocalIdx_1+1) % Poly->PolyVerts.Num();
	INT NextVert_1 = Poly->PolyVerts(LocalNextIdx_1);


	// determine which next verts are on top of edge poitns of the calc'd adjacent edge join section
	UBOOL bShare0_Next_OnEdgePt = PointsEqualEnough(GetVertLocation(NextVert_0,WORLD_SPACE),EdgePt0,NEAR_EDGE_PT_TOL)||PointsEqualEnough(GetVertLocation(NextVert_0,WORLD_SPACE),EdgePt1,NEAR_EDGE_PT_TOL);
	UBOOL bShare1_Next_OnEdgePt = PointsEqualEnough(GetVertLocation(NextVert_1,WORLD_SPACE),EdgePt0,NEAR_EDGE_PT_TOL)||PointsEqualEnough(GetVertLocation(NextVert_1,WORLD_SPACE),EdgePt1,NEAR_EDGE_PT_TOL);
	check(!bShare0_Next_OnEdgePt || !bShare1_Next_OnEdgePt);

	// if both nexts are not on edge segment points, we want to start with the next vert of the sharing edge which crosses the edgesegment
	if( !bShare0_Next_OnEdgePt && !bShare1_Next_OnEdgePt )
	{
		FVector trash(0.f);
		UBOOL bShare0_Crosses_EdgePt = (PointDistToSegment(EdgePt0,GetVertLocation(SharingEdgeVert_0,WORLD_SPACE),GetVertLocation(NextVert_0,WORLD_SPACE),trash)<NEAR_EDGE_PT_TOL)||(PointDistToSegment(EdgePt1,GetVertLocation(SharingEdgeVert_0,WORLD_SPACE),GetVertLocation(NextVert_0,WORLD_SPACE),trash)<NEAR_EDGE_PT_TOL);
		UBOOL bShare1_Crosses_EdgePt = (PointDistToSegment(EdgePt0,GetVertLocation(SharingEdgeVert_1,WORLD_SPACE),GetVertLocation(NextVert_1,WORLD_SPACE),trash)<NEAR_EDGE_PT_TOL)||(PointDistToSegment(EdgePt1,GetVertLocation(SharingEdgeVert_1,WORLD_SPACE),GetVertLocation(NextVert_1,WORLD_SPACE),trash)<NEAR_EDGE_PT_TOL);

		// if neither edge crosses the overlap segment this is bogus
		if(bShare0_Crosses_EdgePt == bShare1_Crosses_EdgePt)
		{
			return -1;
		}

		if(bShare0_Crosses_EdgePt)
		{
			return LocalIdx_1;
		}
		else
		{
			return LocalIdx_0;
		}
	}
	// if both are not valid, use the vert which is not on an edge pt
	else if(!bShare0_Next_OnEdgePt)
	{
		return LocalIdx_0;
	}
	else
	{
		return LocalIdx_1;
	}
}

/**
 * 
 */
FLOAT PointDistToLineOutT(const FVector &Point, const FVector &Direction, const FVector &Origin, FVector& OutClosestPoint, FLOAT& out_T)
{
	const FVector SafeDir = Direction.SafeNormal();
	out_T = ((Point-Origin) | SafeDir);
	OutClosestPoint = Origin + (SafeDir * out_T);
	return (OutClosestPoint-Point).Size();
}

/**
 * used during concave poly merging (TryCOmbinePolysConcave)
 * adds verts from Poly to a the vertlist representing the new combined poly.  
 * will remove duplicate verts, and redundant verts
 * @param Poly - poly to add verts from
 * @param StartingLocalVertIdx - local vertIdx to start adding verts from
 * @param out_NewPolyVertIDs - out var of array representing new combined poly 
 */
void UNavigationMeshBase::AddVertsToCombinedPolyForConcaveMerge( FNavMeshPolyBase* Poly, FNavMeshPolyBase* OtherPoly, INT StartingLocalVertIdx, TArray<VERTID>& out_NewPolyVertIDs )
{
	static FLOAT ThreshXY = 1.0f;
	static FLOAT ThreshZ = NAVMESHGEN_MAX_STEP_HEIGHT;
#define MAX_DIST_OFF_LINE ThreshXY
#define MAX_DIST_OFF_LINE_VERT ThreshZ
	
	static FLOAT Dupe_ThreshXY = 1.0f;
#define MAX_DIST_OFF_LINE_FOR_DUPES Dupe_ThreshXY
#define MAX_DIST_OFF_LINE_FOR_DUPES_VERT NAVMESHGEN_MAX_STEP_HEIGHT*2.0f

	for(INT VertAddIdx=0;VertAddIdx<Poly->PolyVerts.Num();++VertAddIdx)
	{
 		VERTID Current_VertID = Poly->PolyVerts((StartingLocalVertIdx+VertAddIdx) % Poly->PolyVerts.Num());
	
		out_NewPolyVertIDs.AddItem(Current_VertID);
	}

	// now remove adjacent duplicates if doing so doesn't change the line of the poly
	INT DupeRemoveIdx=0;
	while(TRUE)
	{
		if(DupeRemoveIdx>=out_NewPolyVertIDs.Num())
		{
			break;
		}

		VERTID Current_VertID = out_NewPolyVertIDs(DupeRemoveIdx);

		INT LocalNextIdx = (DupeRemoveIdx+1) % out_NewPolyVertIDs.Num();
		VERTID Next_VertID = out_NewPolyVertIDs(LocalNextIdx);
		VERTID PrevID = out_NewPolyVertIDs((DupeRemoveIdx-1<0) ? out_NewPolyVertIDs.Num()-1 : DupeRemoveIdx-1);

		if(PrevID == Next_VertID)
		{
			out_NewPolyVertIDs.Remove(DupeRemoveIdx);
			DupeRemoveIdx=0;
			continue;
		}
	
		// if it's an adjacent duplicate, check to see if this location is necessary for the shape of the poly, if so remove one of the dupes, if not remove both
		INT NumDupes=0;
		if(Current_VertID == Next_VertID)
		{
			// find the first vert which is not a dupe
			while(Next_VertID == Current_VertID)
			{
				LocalNextIdx = (LocalNextIdx+1) % out_NewPolyVertIDs.Num();
				Next_VertID = out_NewPolyVertIDs( LocalNextIdx );
				if(++NumDupes >= out_NewPolyVertIDs.Num())
				{
					debugf(TEXT("Poly with all the same vert?!"));
					break;
				}
			}

		}

		FVector CurrentLoc = GetVertLocation(Current_VertID , LOCAL_SPACE);
		FVector PrevLoc = GetVertLocation( PrevID , LOCAL_SPACE );
		FVector NextLoc = GetVertLocation( Next_VertID , LOCAL_SPACE );
		const FVector CurrentLocWithZ = CurrentLoc;
		const FVector PrevLocWithZ = PrevLoc;
		const FVector NextLocWithZ = NextLoc;

		CurrentLoc.Z = PrevLoc.Z = NextLoc.Z = 0.f;

		FVector Closest(0.f);
		FLOAT T=0.f;
		FLOAT OffLineDistXY = PointDistToLineOutT(CurrentLoc,(NextLoc-PrevLoc),PrevLoc,Closest,T);

		const FVector DirWithZ = (NextLocWithZ - PrevLocWithZ).SafeNormal(); 
		FVector ClosestWithZ= PrevLocWithZ + DirWithZ * T;
		FLOAT OffLineDistZ  = Abs<FLOAT>(CurrentLocWithZ.Z-ClosestWithZ.Z);
		
		FLOAT OffLineThresholdXY = (NumDupes>0)? MAX_DIST_OFF_LINE_FOR_DUPES : MAX_DIST_OFF_LINE;
		FLOAT OffLineThresholdZ = (NumDupes>0)? MAX_DIST_OFF_LINE_FOR_DUPES_VERT : MAX_DIST_OFF_LINE_VERT;

		if( OffLineDistXY < OffLineThresholdXY && OffLineDistZ < OffLineThresholdZ)
		{
			out_NewPolyVertIDs.Remove(DupeRemoveIdx,NumDupes+1);
			DupeRemoveIdx=0;
			continue;
		}
		else if (NumDupes>0) // if it's needed for the shape of the poly, leave one of the dupes behind
		{
			out_NewPolyVertIDs.Remove(DupeRemoveIdx,NumDupes);
			DupeRemoveIdx=0;
			continue;
		}

		DupeRemoveIdx++;

		if( DupeRemoveIdx>=out_NewPolyVertIDs.Num())
		{
			break;
		}
	}
}




/*************************************************************
 - Concave polygon decompisition - 
   this code performs an A* search using possible split configurations of a concave
   polygon as the sucessors in the graph
   e.g. find the best configuration of splits between vertexes of a concave polygon to build 
        a set of convex polygons in its place
 **************************************************************/

/**
 *	Struct that stores the state of decompisition for the search (this is the basic search node)
 *  stores a list of convex polys that have been split off of the starting concave poly, and 
 *  the remaining portion of the concave poly
 */
struct FDecompositionState
{
	void InitFrom(const FDecompositionState& Other)
	{
		ConvexPolys = Other.ConvexPolys;
		ActualCostG=0.f;
		HeuristicCostH=0.f;
	}
	FDecompositionState()
	{
		ActualCostG=0.f;
		HeuristicCostH=0.f;
	}

	FDecompositionState(const FDecompositionState& Other)
	{
		ActualCostG=Other.ActualCostG;
		HeuristicCostH=Other.HeuristicCostH;
		ConvexPolys = Other.ConvexPolys;
		ConcavePoly = Other.ConcavePoly;
	}

	UBOOL operator==(const FDecompositionState& Other) const
	{
		return ( ConcavePoly==Other.ConcavePoly && ConvexPolys.Num() == Other.ConvexPolys.Num() );
	}

	TArray< TArray<VERTID> > ConvexPolys;
	TArray< VERTID > ConcavePoly;
	FLOAT ActualCostG;
	FLOAT HeuristicCostH;

	FLOAT GetCost() const
	{
		return ActualCostG+HeuristicCostH;
	}

	/**
	 *	this will compute the heuristic and actual cost for the A* search
	 *  heuristic is based on the size of the convex primitive that is being considered to use as a split
	 *  (e.g. the bigger the primitive the less primitives are necessary)
	 * @param NewConvexShapeSize - the size of the convex primitive we are splitting for this configuration
	 *
	 */
	void ComputeCost(INT NewConvexShapeSize)
	{
		// H = max( 2, ConcaveNumVerts/(NewConvexShapeSize-1) )
		HeuristicCostH = Max<INT>( 2, ConcavePoly.Num()/(NewConvexShapeSize-1) );
		ActualCostG = ConvexPolys.Num();
	}
};

/**
 *	splits the passed poly into two polys along the split line described by two verts already in the poly (SplitVertA/B)
 * @param Poly - the poly to split
 * @param SplitVertA - first index of vert from Poly to split with
 * @param SplitVerB - second index of vert from Poly to split with
 * @param out_NewPolyA - resulting poly from split
 * @param out_NewPolyB - resulting poly from split
 */
void SplitPolyAtLocalVertIndexes( const TArray<VERTID>& Poly, INT SplitVertAIdx, INT SplitVertBIdx, TArray< VERTID >& out_NewPolyA, TArray< VERTID >& out_NewPolyB)
{
	UBOOL bAddingToA=TRUE;
	for(INT PolyVertIdx=0;PolyVertIdx<Poly.Num();++PolyVertIdx)
	{
		VERTID CurrentVert = Poly(PolyVertIdx);
		if(PolyVertIdx==SplitVertAIdx || PolyVertIdx==SplitVertBIdx)
		{
			out_NewPolyA.AddItem(CurrentVert);
			out_NewPolyB.AddItem(CurrentVert);
			bAddingToA = !bAddingToA;
		}
		else
		{
			if(bAddingToA)
			{
				out_NewPolyA.AddItem(CurrentVert);
			}
			else
			{
				out_NewPolyB.AddItem(CurrentVert);
			}
		}
	}
}

#define TARRAY_OPENLIST 1
#if TARRAY_OPENLIST
	typedef TArray< FDecompositionState > OpenListType;
#else
	typedef TDoubleLinkedList< FDecompositionState > OpenListType;
#endif

typedef TLookupMap< FDecompositionState > ClosedListType;

/**
 *	iterates over the working set and finds the cheapest one, pops it from the list, and returns 
 * @param OpenList - list to find the best (cheapest) option from
 * @param ClosedList - closed list (already explored) 
 * @param out_BestState - the best state we just popped
 * @return - TRUE if we found a state to pop (FALSE if the list was empty)
 */
IMPLEMENT_COMPARE_CONSTREF( FDecompositionState, DecompBestLast, { return B.GetCost() - A.GetCost(); } )
UBOOL PopBestState(OpenListType& OpenList,ClosedListType& ClosedList, FDecompositionState& out_BestState)
{
#if TARRAY_OPENLIST
	if( OpenList.Num() > 0 )
	{
		if( OpenList.Num() > 1 )
		{
			Sort<USE_COMPARE_CONSTREF(FDecompositionState,DecompBestLast)>(&OpenList(0),OpenList.Num());
		}

		out_BestState = OpenList.Pop();
		return TRUE;
	}
#else
	OpenListType::TDoubleLinkedListNode* BestNode=NULL;
	for(OpenListType::TIterator It(OpenList.GetHead());It;++It)
	{
		if(BestNode == NULL || BestNode->GetValue().GetCost() > (*It).GetCost())
		{
			BestNode = It.GetNode();
		}
	}

	if(BestNode != NULL)
	{
		out_BestState=BestNode->GetValue();
		OpenList.RemoveNode(BestNode);
		return TRUE;
	}
#endif
	
	return FALSE;
}

/**
 * find a list of inflection points (e.g. concavity transition points) on concave portion of poly
 * @param Mesh - mesh that owns the poly 
 * @param ConcaveShape - array of vertex IDs representing the concave shape
 * @param out_InflectionPoints - out array which will get stuffed with indexes into the concaveshape array of inflection points
 *                               this is indexed into the concaveshape array instead of just using the vertex ID because there could be multiple instances of the vertexID in the shape
 *	
 * @param bUseAllVerts - if TRUE all verts will be used rather than just inflection verts
 */
void FindInflectionVerts(UNavigationMeshBase* Mesh, const TArray<VERTID>& ConcaveShape, TArray<INT>& out_InflectionPoints, UBOOL bUseAllVerts)
{
	static UBOOL bForceAllVerts = FALSE;

	if( bForceAllVerts )
	{
		bUseAllVerts=TRUE;
	}
	if( !bUseAllVerts )
	{
		// MT-> to handle inverted surfaces etc, could change this to CalcNormal().. don't need that right now so avoid the expense
		const FVector Nominal_Normal = FVector(0.f,0.f,1.f);

		for(INT ConcaveVertIdx=0;ConcaveVertIdx<ConcaveShape.Num();++ConcaveVertIdx)
		{
			const FVector Next = Mesh->GetVertLocation(  ConcaveShape((ConcaveVertIdx + 1) % ConcaveShape.Num())  ,LOCAL_SPACE);
			const FVector Prev = Mesh->GetVertLocation(  ConcaveShape((ConcaveVertIdx==0) ? ConcaveShape.Num()-1 : ConcaveVertIdx-1)  ,LOCAL_SPACE);
			const FVector Current = Mesh->GetVertLocation(ConcaveShape(ConcaveVertIdx));

			FLOAT CrossDot = ((Next-Current)^(Prev-Current)) | Nominal_Normal;
			if(CrossDot < 0.f)
			{
				out_InflectionPoints.AddItem(ConcaveVertIdx);
			}
		}	

		
	}
	
	if( bUseAllVerts || out_InflectionPoints.Num() == 0 )
	{
		for(INT ConcaveVertIdx=0;ConcaveVertIdx<ConcaveShape.Num();++ConcaveVertIdx)
		{
			out_InflectionPoints.AddItem(ConcaveVertIdx);
		}
	}
}

/**
 *	testes if a line segment represented by two vertex IDs intersects with any edges in the existing polygon
 *  @param Mesh - the mesh that owns the poly and verts we're testing 
 *  @param Vert0 - ID of first vertex
 *  @param Vert1 - ID of second vertex
 *  @param ConcaveShape - array of vertex IDs representing the polygon we're testing edges for
 *  @param bCheckVertexOnEdge - when TRUE (default) each vert will be tested for proximity to the new split
 *  @return TRUE if the vertex-line-segment intersects an edge in the concave shape
 */
UBOOL DoesSplitIntersectExistingEdge(UNavigationMeshBase* Mesh, VERTID Vert0, VERTID Vert1, const TArray<VERTID>& ConcaveShape, UBOOL bCheckVertexOnEdge)
{
	const FVector Vert0Loc = Mesh->GetVertLocation(Vert0,LOCAL_SPACE);
	const FVector Vert1Loc = Mesh->GetVertLocation(Vert1,LOCAL_SPACE);

	FVector Vert0Loc_NoZ = Vert0Loc;
	FVector Vert1Loc_NoZ = Vert1Loc;
	Vert0Loc_NoZ.Z = 0.f;
	Vert1Loc_NoZ.Z = 0.f;

	for(INT ConcaveIdx=0;ConcaveIdx<ConcaveShape.Num();++ConcaveIdx)
	{
		
		FVector CurrentLoc = Mesh->GetVertLocation(ConcaveShape(ConcaveIdx),LOCAL_SPACE);
		
		static FLOAT Tol = 10.f;
		// if this vert is very close to our proposed split line, don't do it cuz it will cause a sliver, nobody likes those
		if( bCheckVertexOnEdge && IsVertexOnEdgeSegment(CurrentLoc,Vert0Loc,Vert1Loc,TRUE,Tol) )
		{
			return TRUE;
		}

		FVector NextLoc = Mesh->GetVertLocation(ConcaveShape((ConcaveIdx+1)%ConcaveShape.Num()),LOCAL_SPACE);

		CurrentLoc.Z = 0.f;
		NextLoc.Z = 0.f;

		FVector SegmentClosest(0.f),PolyEdgeClosest(0.f);
		SegmentDistToSegmentSafe(Vert0Loc_NoZ,Vert1Loc_NoZ,CurrentLoc,NextLoc,SegmentClosest,PolyEdgeClosest);
		if( (SegmentClosest-PolyEdgeClosest).SizeSquared() < 1.0f)
		{
			if( !SegmentClosest.Equals(Vert0Loc_NoZ,0.1f) && !SegmentClosest.Equals(Vert1Loc_NoZ,0.1f) )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
 *	Hash function for decomposition states
 *   String-style hash based on the vertIDs of the concave portion of the state as that's the truly unique part
 *   and then just add the number of convex primitives after that as we only care about number of convex primitives
 *   a state could have the same number but be slightly different (e.g. order is changed) and is still considered equal
 */
DWORD GetTypeHash(const FDecompositionState& Config)
{	
	DWORD Hash = 5381;

	// compute hash for concave portion	
	for(INT ConcaveIdx=0;ConcaveIdx<Config.ConcavePoly.Num();++ConcaveIdx)
	{
		Hash = ((Hash << 5) + Hash) + Config.ConcavePoly(ConcaveIdx); /* hash * 33 + c */
	}

	Hash = ((Hash << 5) + Hash) + Config.ConvexPolys.Num(); /* hash * 33 + c */

	return Hash;
}

/**
 *	will add the passed state to the open list, but only if it's not already in the closed list
 * @param OpenList - ref to the current working set of the A* search
 * @param ClosedList - ref to the closed list of the A* search
 * @param NewConvexSize - size (number of verts) of the new convex primitive that just got clipped off to form this state (used to calculate the heuristic)
 * @param NewState - the state to add to the open list (if it's not there already)
 */

void AddStateToOpen(OpenListType& OpenList,ClosedListType& ClosedList, INT NewConvexSize, FDecompositionState& NewState)
{
	NewState.ComputeCost(NewConvexSize);

	if( ! ClosedList.HasKey(NewState))
	{
#if TARRAY_OPENLIST
		OpenList.AddItem(NewState);
#else		
		OpenList.AddHead(NewState);
#endif

		ClosedList.AddItem(NewState);
	}
}


/**
 *	will increment the passed value in either direction, wrapping when necessary
 * @param OldIdx - index to increment
 * @Param Direction - direction to increment in (e.g -1, or 1)
 * @param NumElements - size to wrap around (e.g. size of the array)
 * @return the wrapped, incremented index
 */
INT Increment(INT OldIdx,INT Direction, INT NumElements)
{
	INT NewIdx = OldIdx + Direction;
	if(NewIdx < 0) { NewIdx = NumElements-1; }
	else if(NewIdx >= NumElements) { NewIdx = 0;}
	return NewIdx;
}

/**
 * verifies that the incoming poly it is wound in CW order	
 * @param Poly - poly to verify (VERTIDs that represent the poly)
 * @param Mesh - mesh that owns the poly
 */
UBOOL VerifyWinding(const TArray<VERTID>& Poly, UNavigationMeshBase* Mesh)
{
	if(Poly.Num() < 3)
	{
		return FALSE;
	}

	FLOAT TotalArea = 0.f;
	for(INT PolyVertIdx=0;PolyVertIdx<Poly.Num();++PolyVertIdx)
	{
		const FVector Prev = Mesh->GetVertLocation(Poly((PolyVertIdx==0) ? Poly.Num() - 1 : PolyVertIdx-1),LOCAL_SPACE);
		const FVector Current = Mesh->GetVertLocation(Poly(PolyVertIdx),LOCAL_SPACE);
		const FVector Next = Mesh->GetVertLocation(Poly( (PolyVertIdx+1) % Poly.Num()),LOCAL_SPACE);

		TotalArea += TriangleArea2(Prev,Current,Next);

	}
	

	return TotalArea > 0.f;
}

/**
 * verifies that a corner of the poly is not degenerate
 * @param Poly - poly to verify (VERTIDs that represent the poly)
 * @param Mesh - mesh that owns the poly
 */
UBOOL VerifyPolyNormal(const TArray<VERTID>& Poly, UNavigationMeshBase* Mesh)
{
	static UBOOL bShortCircuit=FALSE;
	if ( bShortCircuit )
	{
		return TRUE;
	}

	if(Poly.Num() < 3)
	{
		return FALSE;
	}

	for(INT PolyVertIdx=0;PolyVertIdx<Poly.Num();++PolyVertIdx)
	{
		const FVector Prev = Mesh->GetVertLocation(Poly((PolyVertIdx==0) ? Poly.Num() - 1 : PolyVertIdx-1),LOCAL_SPACE);
		const FVector Current = Mesh->GetVertLocation(Poly(PolyVertIdx),LOCAL_SPACE);
		const FVector Next = Mesh->GetVertLocation(Poly( (PolyVertIdx+1) % Poly.Num()),LOCAL_SPACE);

		static FLOAT Norm_Tolerance = 0.9999f;
		if ( ((Prev-Current).SafeNormal() | (Next-Current).SafeNormal()) >= Norm_Tolerance )
		{
			return FALSE;
		}

	}
	

	return TRUE;
}

/**
 *	WalkPolyAndTryToSplit
 *   this is called by AddSplitPermutationsToOpenList
 *   and will walk along the boundary of the concave portion of a decompistion state and add potential configurations to the workign 
 *   set as it goes
 * @param Direction - direction to walk (CW or CCW)
 * @param InflectionVerts - array of indexes into the concavepoly's vert array which represent inflection points of that concave poly (inflection point being points that make that poly concave)
 * @param InflectionIdx - Index into InflectionVerts of the inflection vert we are currently testing for splits
 * @param BaseState - decomposition state we're adding successors for
 * @param InflectionID - VERTID of current inflection vertex
 * @param Mesh - the mesh that owns the poly we're decomposing
 * @param OpenList - ref to the working set of the A* search
 * @param ClosedList - ref to the closed list hash of the A* search
 * @return - TRUE If we found a solution
 */
static INT NavMeshEdgeAngle=0;
static INT NavMeshWinding=0;
static INT NavMeshNormalConvex=0;
static INT NavMeshEdgeIsect=0;
static INT NavMeshOther=0;
static INT NavMeshSuccess=0;
static INT NavMeshTotal=0;
UBOOL WalkPolyAndTryToSplit( TArray<INT> &InflectionVerts,
							INT InflectionIdx,
							FDecompositionState &out_BaseState,
							const VERTID InflectionID,
							UNavigationMeshBase* Mesh,
							OpenListType& OpenList,
							ClosedListType& ClosedList)
{
#define CW_DIR 1
#define CCW_DIR -1

	INT InnerConcaveVertIdx = InflectionVerts(InflectionIdx);

	const VERTID Inflection_NextID = out_BaseState.ConcavePoly((InnerConcaveVertIdx+1) % out_BaseState.ConcavePoly.Num());
	const VERTID Inflection_PrevID = out_BaseState.ConcavePoly((InnerConcaveVertIdx==0) ? out_BaseState.ConcavePoly.Num()-1 : InnerConcaveVertIdx-1);
	
	// save off locations of prev,cur,next for inflection vert for edge corner angle testing
	const FVector Current_InflectionVertLoc = Mesh->GetVertLocation(InflectionID,LOCAL_SPACE);
	const FVector Next_InflectionVertLoc = Mesh->GetVertLocation(Inflection_NextID,LOCAL_SPACE);
	const FVector Prev_InflectionVertLoc = Mesh->GetVertLocation(Inflection_PrevID,LOCAL_SPACE);

	UBOOL bHitInflectionPoint = FALSE;
	for(INT InnerLoopCount=0;InnerLoopCount<out_BaseState.ConcavePoly.Num();++InnerLoopCount)	
	{
		NavMeshTotal++;
		InnerConcaveVertIdx = Increment(InnerConcaveVertIdx,CW_DIR,out_BaseState.ConcavePoly.Num());
		const VERTID InnerConcaveVertID = out_BaseState.ConcavePoly(InnerConcaveVertIdx);


		// don't try splitting to ourselves :) 
		if(InflectionID == InnerConcaveVertID)
		{
			continue;
		}

		const VERTID Next_InnerConcaveVertID = out_BaseState.ConcavePoly(Increment(InnerConcaveVertIdx,CW_DIR,out_BaseState.ConcavePoly.Num()));
		const VERTID Prev_InnerConcaveVertID = out_BaseState.ConcavePoly(Increment(InnerConcaveVertIdx,CCW_DIR,out_BaseState.ConcavePoly.Num()));

		static UBOOL bCheckEdgeAngle=TRUE;
		if(bCheckEdgeAngle)
		{
			// if the line between verts is outside the angle formed by the corner then it's either going to intersect an edge or it's coming out the wrong side
			// of the corner.. so skip!
			const FVector Current_InnerVertLoc = Mesh->GetVertLocation(InnerConcaveVertID,LOCAL_SPACE);
			const FVector Next_InnerVertLoc = Mesh->GetVertLocation(Next_InnerConcaveVertID,LOCAL_SPACE);
			const FVector Prev_InnerVertLoc = Mesh->GetVertLocation(Prev_InnerConcaveVertID,LOCAL_SPACE);
			if( !IsWithinEdgeAngle(Current_InflectionVertLoc,Prev_InnerVertLoc,Current_InnerVertLoc,Next_InnerVertLoc) 
				|| !IsWithinEdgeAngle(Current_InnerVertLoc,Prev_InflectionVertLoc,Current_InflectionVertLoc,Next_InflectionVertLoc) )
			{
				NavMeshEdgeAngle++;
				continue;
			}

		}

		// don't try splitting to direct neighbors
		if( Next_InnerConcaveVertID == InflectionID || Prev_InnerConcaveVertID == InnerConcaveVertID || 
						Next_InnerConcaveVertID == InnerConcaveVertID || Prev_InnerConcaveVertID == InflectionID )
		{
			continue;
		}


		// split the poly into two vert arrays
		static TArray<VERTID> PolyA,PolyB;
		PolyA.Reset();
		PolyB.Reset();

		SplitPolyAtLocalVertIndexes(out_BaseState.ConcavePoly,InflectionVerts(InflectionIdx),InnerConcaveVertIdx,PolyA,PolyB);

		// if the winding on either new poly is flipped it means the split crossed the boundary of the poly .. bad things
		if( ! VerifyWinding(PolyA,Mesh) || ! VerifyWinding(PolyB,Mesh) )
		{
			NavMeshWinding++;
			continue;
		}	

		// if neither resulting poly is convex, skip
		const FVector NormA = FNavMeshPolyBase::CalcNormal(PolyA,Mesh,LOCAL_SPACE);
		const FVector NormB = FNavMeshPolyBase::CalcNormal(PolyB,Mesh,LOCAL_SPACE);

		UBOOL bPolyAConvex = PolyA.Num() > 2 && NormA.Z >= NAVMESHGEN_MIN_WALKABLE_Z && Mesh->IsConvex(PolyA,-1.0f,NormA);
		UBOOL bPolyBConvex = PolyB.Num() > 2 && NormB.Z >= NAVMESHGEN_MIN_WALKABLE_Z && Mesh->IsConvex(PolyB,-1.0f,NormB);
		if( !bPolyAConvex && !bPolyBConvex )
		{
			NavMeshNormalConvex++;
			continue;
		}
	

		// if this split intersects existing edges of the poly.. skip it
		if(DoesSplitIntersectExistingEdge(Mesh,InflectionID,InnerConcaveVertID,out_BaseState.ConcavePoly,TRUE))
		{
			NavMeshEdgeIsect++;
			continue;
		}

		checkSlow(PolyA.Num() + PolyB.Num() == out_BaseState.ConcavePoly.Num()+2);



		INT NewConvexSize=0;
		FDecompositionState NewState;
		NewState.InitFrom(out_BaseState);

		FLOAT PolyA_Area = 0.f;
		if( bPolyAConvex )
		{
			PolyA_Area = FNavMeshPolyBase::CalcArea(PolyA,Mesh);
			if( NormA.IsNearlyZero() || NormA.Z < NAVMESHGEN_MIN_WALKABLE_Z || PolyA_Area < KINDA_SMALL_NUMBER || !VerifyPolyNormal(PolyA,Mesh) )
			{
				continue;
			}

			NewConvexSize=PolyA.Num();
			NewState.ConvexPolys.AddItem(PolyA);
		}
		else
		{
			NewState.ConcavePoly = PolyA;
		}

		FLOAT PolyB_Area = 0.f;
		if( bPolyBConvex )
		{
			PolyB_Area = FNavMeshPolyBase::CalcArea(PolyB,Mesh);
			if( NormB.IsNearlyZero() || NormB.Z < NAVMESHGEN_MIN_WALKABLE_Z || PolyB_Area < KINDA_SMALL_NUMBER || !VerifyPolyNormal(PolyB,Mesh) )
			{				
				continue;
			}
			NewConvexSize=PolyB.Num();
			NewState.ConvexPolys.AddItem(PolyB);
		}
		else
		{
			NewState.ConcavePoly = PolyB;
		}

		AddStateToOpen(OpenList,ClosedList,NewConvexSize,NewState);

		if( bPolyBConvex && bPolyAConvex )
		{
			out_BaseState=NewState;
			NavMeshSuccess++;
			return TRUE;
		}
		else
		{
			NavMeshOther++;
		}
	}

	return FALSE;
}

/**
 *  AddSplitPermutationsToOpenList
 *	this adds successors to the A* search of the best decomposition of a slab
 *  it first finds "inflection points" or verts that make the poly concave, and then tries to link from these vertexes 
 *  to any other verts that form convex polys and don't break other rules that would create badness (e.g. crossing other edges)
 *  It does this by walking from each inflection point along the boundary of the remainign concave poly in both directions adding
 *  possible configurations as it goes
 * @param Mesh - the mesh holding the poly we're decompising
 * @param OpenList - ref to the current working set of the A* search
 * @param ClosedList - ref to the current closed list hash
 * @param BaseState - decompisition state we're adding sucessors for
 * @param bUseAllVerts - whether or not to tell findinflectionverts to use ALL verts isntead of just inflection 
 * @return TRUE if we found a full solution
 */
UBOOL AddSplitPermutationsToOpenList(UNavigationMeshBase* Mesh, OpenListType& OpenList, ClosedListType& ClosedList, FDecompositionState& out_BaseState, UBOOL bUseAllVerts)
{
	FDecompositionState NewState;

	static TArray<INT> InflectionVerts;
	InflectionVerts.Reset();
	FindInflectionVerts(Mesh,out_BaseState.ConcavePoly,InflectionVerts, bUseAllVerts);

	NavMeshEdgeAngle=0;
	NavMeshWinding=0;
	NavMeshNormalConvex=0;
	NavMeshEdgeIsect=0;
	NavMeshOther=0;	
	NavMeshTotal=0;
	NavMeshSuccess=0;
	// for each inflection point find all valid split verts in the rest of the poly
	for(INT InflectionIdx=0;InflectionIdx<InflectionVerts.Num();++InflectionIdx)
	{
		const VERTID InflectionID = out_BaseState.ConcavePoly(InflectionVerts(InflectionIdx));
		// walk from inflection point and link to verts
		if ( WalkPolyAndTryToSplit(InflectionVerts, InflectionIdx, out_BaseState, InflectionID, Mesh, OpenList, ClosedList) )
		{
			return TRUE;
		}
	}

	static UBOOL bPrint = FALSE;
	if( bPrint )
	{
		debugf(TEXT("AddSplitPermutationsToOpenList: EdgeAngle: %i Winding: %i Normal/Convex: %i EdgeIsect: %i Other: %i \nSuccess: %i Total: %i"),
			NavMeshEdgeAngle,
			NavMeshWinding,
			NavMeshNormalConvex,
			NavMeshEdgeIsect,
			NavMeshOther,
			NavMeshSuccess,
			NavMeshTotal);

	}


	return FALSE;
}
/**
 *	will decompose a concave poly into a list of convex ones
 * @param Poly - the (presumably concave) poly to decompose into convex polys
 * @param bPartialFail - indicates this is the result of a previous decomposition attempt so we should handle it specially
 * @return - the number of polys generated for the start poly
 */

INT UNavigationMeshBase::DecomposePolyToConvexPrimitives( FNavMeshPolyBase* Poly, INT FailDepth )
{
	if( Poly->PolyVerts.Num() < 3 )
	{
		return -1;
	}

	if(IsConvex(Poly->PolyVerts))
	{	
		return 1;
	}


	static INT MaxFailDepth = 3;
	#define MAX_FAIL_DEPTH MaxFailDepth

	// if this is the cast off of a previous attempt, add verts in the middle of all edges to hopefully allow for some better split opportunities
	if( FailDepth < MAX_FAIL_DEPTH && FailDepth > -1 )
	{
		INT AddedVerts=0;

		if ( FailDepth == 0 )
		{

			// add inflection verts near other verts to help out splitting on narrow shapes
			for(INT OuterVertIdx=Poly->PolyVerts.Num()-1;OuterVertIdx>=0;--OuterVertIdx)
			{
				const FVector OuterVert = GetVertLocation(Poly->PolyVerts(OuterVertIdx),LOCAL_SPACE);

				INT OuterPrev = (OuterVertIdx+1)%Poly->PolyVerts.Num();

				for(INT VertIdx=0;VertIdx<Poly->PolyVerts.Num();++VertIdx)
				{
					INT NextIdx = (VertIdx+1) % Poly->PolyVerts.Num();
					INT PrevIdx = (VertIdx-1 < 0)? Poly->PolyVerts.Num()-1 : VertIdx-1;
					if( VertIdx == OuterVertIdx || NextIdx == OuterVertIdx || PrevIdx == OuterVertIdx )
					{
						continue;
					}
					const FVector Current = GetVertLocation(Poly->PolyVerts(VertIdx),LOCAL_SPACE);
					const FVector Next = GetVertLocation(Poly->PolyVerts( NextIdx ),LOCAL_SPACE);
					//const FVector Prev = GetVertLocation(Poly->PolyVerts( PrevIdx ));

					if( IsVertexOnEdgeSegment(OuterVert,Next,Current,TRUE,NAVMESHGEN_STEP_SIZE) )
					{
						FVector ClosestPt(0.f);

						PointDistToSegment(OuterVert,Current,Next,ClosestPt);
						VERTID NewVert = AddVert(ClosestPt,LOCAL_SPACE);
						if( !Poly->PolyVerts.ContainsItem(NewVert) )
						{
							++AddedVerts;

							Poly->PolyVerts.InsertItem(NewVert,NextIdx);
							break;
						}
					}

				}
			}

			debugf(TEXT("Added %i inflection verts"),AddedVerts);
		}
		else
		{

			for( INT PolyVertIdx=Poly->PolyVerts.Num()-1; PolyVertIdx >= 0; --PolyVertIdx)
			{
				INT PrevIdx = (PolyVertIdx+1) % Poly->PolyVerts.Num();
				const FVector VertLoc = Verts(Poly->PolyVerts(PolyVertIdx));
				const FVector PrevLoc = Verts(Poly->PolyVerts(PrevIdx));

				if( (VertLoc-PrevLoc).SizeSquared() < NAVMESHGEN_STEP_SIZE * NAVMESHGEN_STEP_SIZE )
				{
					continue;
				}
				const FVector MidPt = (VertLoc + PrevLoc) * 0.5f;
				
				++AddedVerts;
				VERTID NewVert = AddVert(MidPt,LOCAL_SPACE);
				Poly->PolyVerts.InsertItem(NewVert,PrevIdx);
				if( PrevIdx < PolyVertIdx )
				{
					++PolyVertIdx;
				}
			}

			// if we didn't add any new verts, no point in trying again
			if ( AddedVerts == 0 )
			{
				return -1;
			}
		}
	}


	//SCOPE_QUICK_TIMERX(Decomposition)
	// Working set
	OpenListType OpenList;
	static INT OpenListCap = 1500;
	static INT OpenListReAddMax = 100;
	OpenList.Empty(OpenListCap+OpenListReAddMax);

	ClosedListType ClosedList;

	// add default state to open list 
	FDecompositionState NewState;	
	NewState.ConcavePoly = Poly->PolyVerts;
	NewState.ActualCostG=0.f;
	NewState.HeuristicCostH=0.f;
#if TARRAY_OPENLIST
	OpenList.AddItem(NewState);
#else
	OpenList.AddHead(NewState);
#endif

	// main loop
	FDecompositionState CurrentState;
	UBOOL bFoundSolution = FALSE;

	INT Iterations=0;

	while(PopBestState(OpenList,ClosedList,CurrentState))
	{

		if( OpenList.Num() > OpenListCap )
		{
#if TARRAY_OPENLIST
			OpenList.Reset();
#else			
			OpenList.Clear();
#endif
			ClosedList.Empty(ClosedList.Num());
		}

		// add all split permutations to the open list 
		if( AddSplitPermutationsToOpenList(this,OpenList,ClosedList,CurrentState, FailDepth>=0) )
		{
			bFoundSolution=TRUE;
			break;
		}

		Iterations++;		

	}


	INT StartingVerts = Poly->PolyVerts.Num();
	FLOAT PolyHeight = Poly->GetPolyHeight();
	FVector PolyNorm = Poly->GetPolyNormal(LOCAL_SPACE);
	INT NewPolyCount=0;
	TArray<FNavMeshPolyBase*> NewPolys;

	RemovePoly(Poly);
	for(INT NewPolyIdx=0;NewPolyIdx<CurrentState.ConvexPolys.Num();++NewPolyIdx)
	{
		NewPolys.AddItem(AddPolyFromVertIndices(CurrentState.ConvexPolys(NewPolyIdx),PolyHeight));
	}

	NewPolyCount = NewPolys.Num();

	UBOOL bTriangulate = FALSE;

	if( !bFoundSolution )
	{


		if( FailDepth < MAX_FAIL_DEPTH )
		{
			// NOTE: if you see this message and the path simplification is taking a really long time.  Try taking the pylon and breaking it up into
			// a number of smaller pylons so that the area covered is smaller per pylon
			debugf(TEXT("Attempting to reconcile remaining %i vert concave slab from poly (Created %i convex shapes so far from %i iterations)... (For Pylon: %s)"),CurrentState.ConcavePoly.Num(),CurrentState.ConvexPolys.Num(), Iterations, *GetPylon()->GetFullName() );
			FNavMeshPolyBase* ConcaveRemains = AddPolyFromVertIndices(CurrentState.ConcavePoly,PolyHeight);
			INT NumAdded = DecomposePolyToConvexPrimitives(ConcaveRemains,FailDepth+1);
			if (NumAdded >= 0)
			{
				NewPolyCount += NumAdded;
			}
			else
			{
				RemovePoly(ConcaveRemains);
				bTriangulate = TRUE;
			}
		}
		else
		{
			bTriangulate = TRUE;
		}

		if( bTriangulate )
		{

	#if !FINAL_RELEASE
			debugf(TEXT("WARNING! could not decompose polygon after %i iterations!!, triangulating remaining %i sized poly instead! (For Pylon: %s)"),Iterations,CurrentState.ConcavePoly.Num(), *GetPylon()->GetFullName());
	#endif

			TArray<VERTID> VertexBuffer;
			TriangulatePoly(CurrentState.ConcavePoly,PolyNorm,VertexBuffer);
			for(INT TriIdx=0;TriIdx<VertexBuffer.Num();TriIdx += 3)
			{
				TArray<VERTID> NewPoly;
				NewPoly.AddItem(VertexBuffer(TriIdx));
				NewPoly.AddItem(VertexBuffer(TriIdx+1));
				NewPoly.AddItem(VertexBuffer(TriIdx+2));

				if( IsConvex(NewPoly) == TRUE )
				{
					NewPolys.AddItem(AddPolyFromVertIndices(NewPoly,PolyHeight));
				}
/*
				else
				{
					FVector v0 = GetVertLocation(NewPoly(0));
					FVector v1 = GetVertLocation(NewPoly(1));
					FVector v2 = GetVertLocation(NewPoly(2));
					FVector Ctr = (v0 + v1 + v2) * 0.333f;

					GWorld->GetWorldInfo()->DrawDebugLine(v0,v1,0,255,255,TRUE);
					GWorld->GetWorldInfo()->DrawDebugLine(v1,v2,0,255,255,TRUE);
					GWorld->GetWorldInfo()->DrawDebugLine(v2,v0,0,255,255,TRUE);
					GWorld->GetWorldInfo()->DrawDebugLine(Ctr,Ctr+FVector(0.f,0.f,100.f),0,255,255,TRUE);
				}
*/

			}
		}
	}
	
	// after all polys have been added, do another edge simplification pass on them
	for(INT NewPolyIdx=0;NewPolyIdx<NewPolys.Num();++NewPolyIdx)
	{
		SimplifyEdgesOfPoly(NewPolys(NewPolyIdx));
	}

#if !FINAL_RELEASE
	static UBOOL bDrawDebug=FALSE;
	if(bDrawDebug && !bFoundSolution)
	{
		debugf(TEXT("Decomposition FAILED!.  Iterations: %i Starting verts: %i ClosedListSize: %i OpenListSize: %i"),Iterations, StartingVerts,ClosedList.Num(),OpenList.Num());

		//Poly->DrawPoly(GWorld->PersistentLineBatcher,FColor(255,255,255),FVector(0,0,10));

		static INT DebugDrawCount = 0;
		if( DebugDrawCount != 0 )
		{	
			for(INT ConvexIdx=0;ConvexIdx<CurrentState.ConvexPolys.Num();++ConvexIdx)
			{
				FColor C = FColor::MakeRedToGreenColorFromScalar((FLOAT)ConvexIdx/(FLOAT)CurrentState.ConvexPolys.Num());
				for(INT PolyVertIdx=0;PolyVertIdx<CurrentState.ConvexPolys(ConvexIdx).Num(); ++PolyVertIdx)
				{
					const FVector Current = GetVertLocation(CurrentState.ConvexPolys(ConvexIdx)(PolyVertIdx))+VRand();
					const FVector Next = GetVertLocation(CurrentState.ConvexPolys(ConvexIdx)( (PolyVertIdx+1) % CurrentState.ConvexPolys(ConvexIdx).Num()))+VRand();

					GWorld->GetWorldInfo()->DrawDebugLine(Current,Next,C.R,C.G,C.B,TRUE);
				}

				if( DebugDrawCount > -1 && ConvexIdx >= DebugDrawCount )
				{
					break;
				}
			}
		}

		
		static INT MinVertCount = 5;
		if( CurrentState.ConcavePoly.Num() > MinVertCount )
		{			
			static INT DebugCnt = 0;
			static INT DebugIdx = -1;
			if( bTriangulate && (DebugCnt++ == DebugIdx || DebugIdx == -1))
			{				
				FColor C = FColor::MakeRandomColor();

				for(INT ConcaveIdx=0;ConcaveIdx<CurrentState.ConcavePoly.Num();++ConcaveIdx)
				{
					const FVector Current = GetVertLocation(CurrentState.ConcavePoly(ConcaveIdx))+FVector(0,0,60);
					const FVector Next = GetVertLocation(CurrentState.ConcavePoly( (ConcaveIdx+1) % CurrentState.ConcavePoly.Num()))+FVector(0,0,60);
					GWorld->GetWorldInfo()->DrawDebugLine(Current,Next,C.R,C.G,C.B,TRUE);
					GWorld->GetWorldInfo()->DrawDebugLine(Current,Current+FVector(0.f,0.f,10.f)+VRand(),0,255,255,TRUE);

				}
			}
		}
	}
#endif 
	
	return NewPolyCount;
}

/**
 * will triangualte the passed poly using ear clipping
 * @param Poly - poly to triangualte 
 * @param out_VertexBuffer - output buffer of triangles created
*/	
void UNavigationMeshBase::TriangulatePoly( FNavMeshPolyBase* Poly, TArray<VERTID>& out_VertexBuffer)
{
	// local copy that we're goign to remove from
	TArray<VERTID> PolyVerts = Poly->PolyVerts;
	TriangulatePoly(Poly->PolyVerts,Poly->GetPolyNormal(LOCAL_SPACE),out_VertexBuffer);
}

/**
 * will triangualte the passed poly using ear clipping
 * @param PolyVerts - vertex list for poly to be triangulated
 * @param PolyNorm - normal of poly we're triangulating
 * @param out_VertexBuffer - output buffer of triangles created
*/	
void UNavigationMeshBase::TriangulatePoly( const TArray< VERTID >& InPolyVerts, const FVector& PolyNorm, TArray<VERTID>& out_VertexBuffer)
{
	TArray<VERTID> PolyVerts = InPolyVerts;
	while(PolyVerts.Num() >= 3)
	{
		UBOOL bSuccessfullyFoundEar=FALSE;

		//for(INT LocalPolyVertIdx=PolyVerts.Num()-1;LocalPolyVertIdx>=0;--LocalPolyVertIdx)	
		for(INT LocalPolyVertIdx=0;LocalPolyVertIdx<PolyVerts.Num();++LocalPolyVertIdx)	
		{
			TArray<VERTID> NewPolyVertIDs;
			NewPolyVertIDs.AddItem(PolyVerts( (LocalPolyVertIdx-1 >= 0)? LocalPolyVertIdx-1 : PolyVerts.Num()-1));
			NewPolyVertIDs.AddItem(PolyVerts( LocalPolyVertIdx ));
			NewPolyVertIDs.AddItem(PolyVerts( (LocalPolyVertIdx + 1) % PolyVerts.Num() ));

			TArray<FVector> NewPolyVertLocs;
			NewPolyVertLocs.AddItem(GetVertLocation(NewPolyVertIDs(0),LOCAL_SPACE));
			NewPolyVertLocs.AddItem(GetVertLocation(NewPolyVertIDs(1),LOCAL_SPACE));
			NewPolyVertLocs.AddItem(GetVertLocation(NewPolyVertIDs(2),LOCAL_SPACE));

			const FVector CrossPdt = (NewPolyVertLocs(1)-NewPolyVertLocs(0)) ^ (NewPolyVertLocs(2)-NewPolyVertLocs(0));
			const FLOAT   CrossDotNorm = CrossPdt | PolyNorm;
			// if these muthas are convex, see if there are any verts inside this tri
			if ( !IsNegativeFloat( CrossDotNorm ) )
			{
				UBOOL bAVertIsInside=FALSE;
				for(INT VertCheckPolyVertIdx=0;VertCheckPolyVertIdx<PolyVerts.Num();++VertCheckPolyVertIdx)
				{
					if ( NewPolyVertIDs.ContainsItem(PolyVerts(VertCheckPolyVertIdx)) )
					{
						continue;
					}

					if (FNavMeshPolyBase::ContainsPoint(NewPolyVertLocs,GetVertLocation(PolyVerts(VertCheckPolyVertIdx),LOCAL_SPACE)))
					{
						bAVertIsInside=TRUE;
						break;
					}
				}

				if(!bAVertIsInside)
				{
					// then we found an ear, remove the center vertex from the list, add the tri to the output buffer and restart
					PolyVerts.Remove(LocalPolyVertIdx);
					if(FNavMeshPolyBase::CalcArea(NewPolyVertIDs,this) >= NAVMESHGEN_MIN_POLY_AREA &&
						FNavMeshPolyBase::CalcNormal(NewPolyVertIDs,this,LOCAL_SPACE).Z >= NAVMESHGEN_MIN_WALKABLE_Z)
					{
						out_VertexBuffer.Append(NewPolyVertIDs);
					}
					bSuccessfullyFoundEar=TRUE;
					break;
				}
			}
		}

		//check(bSuccessfullyFoundEar);
		if(!bSuccessfullyFoundEar)
		{
// 			for(INT LocalPolyVertIdx=PolyVerts.Num()-1;LocalPolyVertIdx>=0;--LocalPolyVertIdx)	
// 			{
// 				GWorld->GetWorldInfo()->DrawDebugLine(GetVertLocation(PolyVerts(LocalPolyVertIdx)),GetVertLocation(PolyVerts(LocalPolyVertIdx))+FVector(0.f,0.f,15.f),255,0,0,TRUE);
// 			}
			break;
		}
	}
}

/**
 * will attempt to combine two polygons by finding an adjacent section and removing interior edges
 * if polys are not adjacent, polys are not compatible slope, or not compatible height this will return NULL
 * @param PolyA - first poly to try and combine
 * @param PolyB - second poly to try and combine
 * @param COntrolPointMap - control point map to test final poly against
 * @return - the new combined poly, or NULL if none was possible
 */
FNavMeshPolyBase* UNavigationMeshBase::TryCombinePolysConcave(
									FNavMeshPolyBase* PolyA,
									FNavMeshPolyBase* PolyB
								        )
{

	// ***  make sure polys are compatible slope/height
	if(!PolysAreCompatibleHeight(PolyA,PolyB))
	{
		return NULL;
	}

	if(!PolysAreCompatibleSlope(PolyA,PolyB,NAVMESHGEN_MERGE_DOT_CONCAVEMERGE))
	{
		return NULL;
	}

	// *** find adjacent edges
	VERTID ASharingEdgeVert_0, ASharingEdgeVert_1, BSharingEdgeVert_0, BSharingEdgeVert_1;
	FVector EdgePt0(0.f), EdgePt1(0.f);

	if(!FindAdjacentEdgeBetweenPolyAandPolyB(PolyA,PolyB,ASharingEdgeVert_0,ASharingEdgeVert_1,BSharingEdgeVert_0,BSharingEdgeVert_1,EdgePt0,EdgePt1))
	{
		return NULL;
	}

	TArray<VERTID> NewPolyVertIDs;
	/* -> Add both polys to overall poly ****************************************
	       Choose StartVert:
					 Start with either SharingEdgeVert_0 or SharingEdgeVert_1 
		             whichever is not one of the EdgePoints
					 if both verts have nexts which are not edgepoints, choose the one which is within the edge segment
		   Add Verts:
					 add all verts from poly, skipping duplicates
    *******************************************************************/
	
	// find starting vert, and add verts for PolyA to combined poly
	INT StartingLocalVertIdx = FindStartingIndex( PolyA, EdgePt0, EdgePt1, ASharingEdgeVert_0, ASharingEdgeVert_1);
	if(StartingLocalVertIdx < 0)
	{
		return NULL;
	}
	
	AddVertsToCombinedPolyForConcaveMerge(PolyA, PolyB, StartingLocalVertIdx, NewPolyVertIDs);

	// find starting vert, and add verts for PolyB to combined poly
	StartingLocalVertIdx = FindStartingIndex( PolyB, EdgePt0, EdgePt1, BSharingEdgeVert_0, BSharingEdgeVert_1);
	if(StartingLocalVertIdx < 0)
	{
		return NULL;
	}

	AddVertsToCombinedPolyForConcaveMerge(PolyB, PolyA, StartingLocalVertIdx, NewPolyVertIDs);

	
	if(NewPolyVertIDs.Num() < 3)
	{
		return NULL;
	}

	FLOAT NewPolyheight = (PolyA->GetPolyHeight() + PolyB->GetPolyHeight()) * 0.5f;

	// *** remove old polys, add new one
	FVector NewBuildLoc = PolyA->PolyBuildLoc;

	RemovePoly(PolyA);
	RemovePoly(PolyB);


	// make sure the counter-clockwise-most vert is at index 0
	FNavMeshPolyBase* NewPoly = AddPolyFromVertIndices(NewPolyVertIDs,NewPolyheight);
	NewPoly->PolyBuildLoc = NewBuildLoc;
	return NewPoly;
}

/**
 *	verifies that if we collapse the corner described by prev,cur,next by removing cur we won't create a hole in the mesh
 * @param Mesh - mesh that owns the corner
 * @param Prev - location in WS of prev vert of corner
 * @param Cur - location in WS of current vert of corner
 * @param Next - location in WS of next vert of corner
 * @return - TRUE if it's safe to collapse
 */
UBOOL IsItSafeToCollapseCorner(UNavigationMeshBase* Mesh, const FVector& Prev, const FVector& Cur, const FVector& Next)
{

	FBox CornerBounds(0);

	CornerBounds += Prev;
	CornerBounds += Cur;
	CornerBounds += Next;
	CornerBounds = CornerBounds.ExpandBy(5.f);

	TArray<VERTID> Verts;
	Mesh->GetAllVertsNearPoint(CornerBounds.GetCenter(),CornerBounds.GetExtent(),Verts);
	

	// if we have any verts in the interior of the edges that form the corner it's not safe to collapse
	for(INT VertIdx=0;VertIdx<Verts.Num();++VertIdx)
	{
		const FVector VertLoc = Mesh->GetVertLocation(Verts(VertIdx),WORLD_SPACE);
		FVector Closest(0.f);
		PointDistToSegment(VertLoc,Prev,Cur,Closest);

		if( (Closest-VertLoc).Size2D()<1.f && Abs<FLOAT>(Closest.Z - VertLoc.Z) < NAVMESHGEN_MAX_STEP_HEIGHT
			&& !Closest.Equals(Cur,0.1f) && !Closest.Equals(Prev,0.1f))
		{
			return FALSE;
		}

		PointDistToSegment(VertLoc,Cur,Next,Closest);
		if( (Closest-VertLoc).Size2D()<1.f && Abs<FLOAT>(Closest.Z - VertLoc.Z) < NAVMESHGEN_MAX_STEP_HEIGHT
			&& !Closest.Equals(Cur,0.1f) && !Closest.Equals(Next,0.1f))
		{
			return FALSE;
		}
	}
	
	return TRUE;
}

/**
 * (wrapper for VerifySlopeAlongTrajectory used by edge simplification to early out when we don't need to do this test
 * used to check for edge simplification isntances where removing a vert would pull the mesh off of geo into open space
 */
UBOOL VerifyTrajectoryForEdgeSimplification(const FVector& Start, const FVector& Dir, FLOAT SweepDistance, AScout* Scout, FLOAT MaxHeightOffset, FVector* ExtentOverride=NULL)
{

	FVector Extent = (ExtentOverride != NULL)? FVector(*ExtentOverride) :  FVector(STEPHEIGHT_TEST_STEP_SIZE);
	// make sure we move at least stepsize horizontally each time
	FLOAT CompensatedStepSize = STEPHEIGHT_TEST_STEP_SIZE / (Dir|Dir.SafeNormal2D());
	if( appCeil(SweepDistance / CompensatedStepSize) > 5 )
	{
		CompensatedStepSize = SweepDistance/5.0f;
	}

	FVector DownOffset = FVector(0.f,0.f,-NAVMESHGEN_MAX_DROP_HEIGHT);

	// now do linechecks down periodically and make sure that we don't exceed max step height at each point
	FLOAT CurDist = 0.f;

	// ** find sane starting position (have to do trace here because the parent poly could already be floating some)
	FCheckResult TempHit(1.f);
	FVector CurTestStartPos = Start;
	FVector End = Start + Dir * SweepDistance;

	CurTestStartPos.Z -= NAVMESHGEN_ENTITY_HALF_HEIGHT - STEPHEIGHT_TEST_STEP_SIZE;
	FVector InitialDownTestStart = CurTestStartPos+FVector(0.f,0.f,NAVMESHGEN_MAX_STEP_HEIGHT);
	FVector InitialDownTestEnd = CurTestStartPos+DownOffset;
	if(!GWorld->SingleLineCheck(TempHit,Scout,InitialDownTestEnd,InitialDownTestStart,EXPANSION_TRACE_FLAGS,Extent))
	{
		CurTestStartPos = TempHit.Location;

		FVector DEBUGSTARTPOS = CurTestStartPos;

		FLOAT PrevZ = CurTestStartPos.Z;

#define	LATERAL_EXTENT_DIFFERENCE 1.0f


		FVector PrevPos = CurTestStartPos;
		while(CurDist <= SweepDistance)
		{
			
			FVector Cp(0.f);
			FCheckResult PCHit(1.0f);
			if(GWorld->SingleLineCheck(TempHit,Scout,CurTestStartPos+DownOffset,CurTestStartPos+FVector(0.f,0.f,NAVMESHGEN_MAX_STEP_HEIGHT+NAVMESHGEN_ENTITY_HALF_HEIGHT),EXPANSION_TRACE_FLAGS,Extent) 
				|| Abs<FLOAT>(TempHit.Location.Z - PrevZ) > NAVMESHGEN_MAX_STEP_HEIGHT
				|| TempHit.Normal.Z < Scout->WalkableFloorZ 
				|| PointDistToSegment( TempHit.Location, Start,End,Cp) > NAVMESHGEN_MAX_STEP_HEIGHT+MaxHeightOffset
				|| !GWorld->SinglePointCheck(PCHit,CurTestStartPos,Extent,EXPANSION_TRACE_FLAGS))
			{
				return FALSE; 
			}
			else
			{			
				PrevZ = TempHit.Location.Z;
				CurDist += CompensatedStepSize;

				PrevPos=CurTestStartPos;
				CurTestStartPos.Z = PrevZ;			
				CurTestStartPos += Dir * CompensatedStepSize;
			}
		}
	}
	else
	{		
		// this means that the poly was floating over nothing, or whatever it is floating over is far above what's below
		// 		GWorld->GetWorldInfo()->DrawDebugLine(InitialDownTestEnd,InitialDownTestStart,255,0,0,TRUE);
		// 		GWorld->GetWorldInfo()->DrawDebugLine(Start,End,255,128,0,TRUE);
		return FALSE;
	}

	return TRUE;
}

/**
 * checks to see if the corner specefied by the three points overlaps any pylons other than current pylon
 * @param prev - first point in corner
 * @param cur - second poitn in corner
 * @param next - third point in corner!
 * @return TRUE if corner overlaps another pylon's bounds
 */
UBOOL IsPointCloseToBoundary(APylon* Pylon, const FVector Pt)
{
	INT AngleIncrement = appTrunc(65536.f / NAVMESHGEN_NUM_ARC_TESTS);

	if( !Pylon->IsPtWithinExpansionBounds(Pt) )
	{
		return TRUE;
	}

	// find ground from this position
	for(INT AngleIdx=0;AngleIdx<NAVMESHGEN_NUM_ARC_TESTS;AngleIdx++)
	{
		FVector TestDir = FVector(1.f,0.f,0.f).RotateAngleAxis(AngleIncrement*AngleIdx,FVector(0.f,0.f,1.f));

		FVector TestLoc = Pt + GetExpansionStepSize(TestDir*2.0f*NAVMESHGEN_STEP_SIZE);

		if ( !Pylon->IsPtWithinExpansionBounds(TestLoc) )
		{
			return TRUE;
		}
	}

	return FALSE;
}

UBOOL DoesCornerOverlapAnotherPylon(const FVector& Prev, const FVector& Cur, const FVector& Next, APylon* CurrentPylon)
{


	if( IsPointCloseToBoundary(CurrentPylon,Prev) || 
		IsPointCloseToBoundary(CurrentPylon,Cur) ||
		IsPointCloseToBoundary(CurrentPylon,Next) )
	{
		return TRUE;
	}

	return FALSE;

}

/**
 * used to remove verts that are unnecessary during poly merge
 * @param NewVertList - list of verts representing the new poly we're simplifying
 * @param ExcludedPolys - list of polys to exclude from 'is point on edge' checks
 * @param Idx           - current index into NewVertList array we're checking currently
 * @param EdgeOffLineTolerance - when considering a vertex, this is the distance off the line between prev<->next under which we will consider removing this vertex
 * @param Normal        - of the polygon we're attempting to smoooth
 * @param bConvexPoly - TRUE if we should assume polys are convex and it's safe to do convex dependent tests)
 * @return - TRUE If succesful
 */
UBOOL UNavigationMeshBase::PerformEdgeSmoothingStep( 				
													const TArray<VERTID>& NewVertList,
													TArray<FNavMeshPolyBase*>& ExcludedPolys,
													const TLookupMap<VERTID>& VertsOnEdges_IncludingEndPoints,
													const TLookupMap<VERTID>& VertsOnEdges_NotIncludingEndPoints,
													INT Idx,
													FLOAT EdgeOffLineTolerance,
													const FVector& Norm, 
													UBOOL bConvexPoly)
	{
	static UBOOL bIsect = TRUE;
	static UBOOL bSlopeStepCheck = TRUE;
	if(NAVMESHGEN_DO_EDGESMOOTHING)
	{
		VERTID NextVert = (Idx+1>=NewVertList.Num()) ? NewVertList(0) : NewVertList(Idx+1);
		VERTID PrevVert = (Idx-1<0) ? NewVertList(NewVertList.Num()-1) : NewVertList(Idx-1); 
		VERTID CurVert = NewVertList(Idx);

		FVector PrevAct = GetVertLocation(PrevVert,LOCAL_SPACE);
		FVector CurrentAct = GetVertLocation(CurVert,LOCAL_SPACE);
		FVector NextAct = GetVertLocation(NextVert,LOCAL_SPACE);


		FVector PolyNorm = FNavMeshPolyBase::CalcNormal(NewVertList,this);
		FVector WS_PolyNorm = L2WTransformNormal(PolyNorm);

		FCheckResult Hit(1.0f);
		// if this would make us concave, check distance from center vert to outer edge
		FVector Closest(0.f);
		PointDistToSegment(CurrentAct,PrevAct,NextAct,Closest);

		FLOAT OffDist = aGetFlattenedDistanceBetweenVects(Closest,CurrentAct,Norm);
		UBOOL bDangNearStraight = (OffDist < 0.1f);
		// if it's a straight line just do it.
		if( bDangNearStraight )
		{
			return TRUE;
		}

		
		if(
			(OffDist < EdgeOffLineTolerance 
			&& !VertsOnEdges_IncludingEndPoints.HasKey(CurVert)
			&& !VertsOnEdges_NotIncludingEndPoints.HasKey(PrevVert)
			&& !VertsOnEdges_NotIncludingEndPoints.HasKey(NextVert))
			)
		{
			const FVector WS_Prev = L2WTransformFVector(PrevAct);
			const FVector WS_Next = L2WTransformFVector(NextAct);
			const FVector WS_Cur  = L2WTransformFVector(CurrentAct);

			if( GetPylon() && 
				DoesCornerOverlapAnotherPylon(WS_Prev,WS_Cur,WS_Next,GetPylon()))
			{
				return FALSE;
			}


			FVector LegC = (WS_Prev-WS_Next);
			FLOAT LegCDist = LegC.Size();
			FVector LegCNorm = LegC/LegCDist;
			FLOAT ZDelta = 0.f;

			APylon* IsectingPylon = NULL;
			FNavMeshPolyBase* IsectingPoly=NULL;

			FLOAT HeightOffset = Max<FLOAT>(NAVMESHGEN_ENTITY_HALF_HEIGHT,MaxStepForSlope(LegCDist));	
			FVector Extent = FVector(1.f);
			if( (!bSlopeStepCheck || VerifyTrajectoryForEdgeSimplification(WS_Next+FVector(0.f,0.f,NAVMESHGEN_ENTITY_HALF_HEIGHT),LegCNorm,LegCDist,FPathBuilder::GetScout(),HeightOffset,&Extent))
				&& IsItSafeToCollapseCorner(this,WS_Prev,WS_Cur,WS_Next)
				)
			{
				if(bConvexPoly)
				{
					TArray<FVector> VertLocs;
					for(INT IsectTestIdx=0;IsectTestIdx < NewVertList.Num();IsectTestIdx++)
					{
						if(IsectTestIdx!=Idx)
						{
							VertLocs.AddItem(GetVertLocation(NewVertList(IsectTestIdx)));
						}
					}

					if(!bIsect || !UNavigationHandle::PolyIntersectsMesh(VertLocs,IsectingPylon,IsectingPoly,&ExcludedPolys))
					{
						return TRUE;
					}
				}
				else if (DoesSplitIntersectExistingEdge(this,PrevVert,NextVert,NewVertList,FALSE))
				{
					return FALSE;
				}
				else
				{
					return TRUE;
				}
			}

		}
	}

	return FALSE;
}

/** 
 * attempt to combine two polys using hertel-mehlhorn poly merging
 * If combination is succesful, delete the two old polys and add a new one for the combined version
 * @param Poly1 - the first poly to be combined (with Poly2)
 * @param Poly2 - the second poly to be combined (with Poly1)
 * @param SharedVert1 - the vert index of the first vertex which makes up the shared edge between these polys
 * @param SharedVert2 - the vert index of the second vertex which makes up the shared edge between these polys
 * @param bIgnoreSlop - don't test for slope differences when trying to combine polys
 * @param AxisMask    - this will be multiplied against all locations during processing, use to specify an axis to be ignored
 * @param ControlPointMap - map of control points which the combined poly must remain close to
 * @param bDoEdgeSmoothing - whether or not to try and remove 'jaggy' edges 
 * @return - return NULL if no combination was possible, or the newly combined polys
 */
FNavMeshPolyBase* UNavigationMeshBase::TryCombinePolys(FNavMeshPolyBase* Poly1, FNavMeshPolyBase* Poly2,VERTID SharedVert1, VERTID SharedVert2, UBOOL bIgnoreSlope,FVector AxisMask,ControlPointMap* CtrlPtMap, UBOOL bDoEdgeSmoothing)
{
	check(Poly1 != NULL);
	check(Poly2 != NULL);
	check(Poly1!=Poly2);
	if(SharedVert1 == MAXVERTID || SharedVert2 == MAXVERTID)
	{
		if(!FindSharedEdge(Poly1,Poly2,this,SharedVert1,SharedVert2))
		{
			return NULL;
		}
	}
	// find the clockwise most of the shared verts in poly1
	INT Poly1ClockwiseMostIdx = -1;
	INT Poly1CCWMostIdx = -1;

	// if the normals don't match, no go
	if(!bIgnoreSlope && !PolysAreCompatibleSlope(Poly1,Poly2) )
	{
		return NULL;
	}

	if(!PolysAreCompatibleHeight(Poly1,Poly2))
	{
		return NULL;
	}

	FLOAT NewPolyheight = (Poly1->GetPolyHeight() + Poly2->GetPolyHeight()) * 0.5f;

	checkSlowish(Poly1->PolyVerts.Num() > 0);
	// see if the edge crosses 0
	if( (Poly1->PolyVerts(0) == SharedVert1 || Poly1->PolyVerts(0) == SharedVert2)
		&&(Poly1->PolyVerts(Poly1->PolyVerts.Num()-1) == SharedVert1 || Poly1->PolyVerts(Poly1->PolyVerts.Num()-1) == SharedVert2) 
		)
	{
		Poly1ClockwiseMostIdx = 0;
	}
	else
	{
		// if the edge doesn't cross 0, then the clockwise most is going to be the last in the vert list
		for(INT Idx=0;Idx<Poly1->PolyVerts.Num();Idx++)
		{
			if(Poly1->PolyVerts(Idx) == SharedVert1 || Poly1->PolyVerts(Idx) == SharedVert2)
			{
				Poly1ClockwiseMostIdx=Idx;
			}
		}
	}

	checkSlowish(Poly1ClockwiseMostIdx >= 0);
	if(Poly1->PolyVerts(Poly1ClockwiseMostIdx) == SharedVert1)
	{
		UBOOL bFound = Poly1->PolyVerts.FindItem(SharedVert2,Poly1CCWMostIdx);
		checkSlowish(bFound);
	}
	else
	{
		UBOOL bFound = Poly1->PolyVerts.FindItem(SharedVert1,Poly1CCWMostIdx);
		checkSlowish(bFound);
	}

	// now construct a new vert list for the merged poly
	TArray<VERTID> NewVertList;

	// first add the verts from poly1 starting with the clockwise most shared vert
	for(INT VertCount=0, Idx=Poly1ClockwiseMostIdx;VertCount<Poly1->PolyVerts.Num();VertCount++,Idx++)
	{
		if(Idx>=Poly1->PolyVerts.Num())
		{
			Idx=0;
		}
		NewVertList.AddItem(Poly1->PolyVerts(Idx));
	}
	//checkSlowish(NewVertList(NewVertList.Num()-1) == Poly1->PolyVerts(Poly1CCWMostIdx));

	if(NewVertList(NewVertList.Num()-1) != Poly1->PolyVerts(Poly1CCWMostIdx))
	{
// 		debugf(TEXT("NO MATCH?!?!?!? LastIn new list: %i CCWMostIdx in Poly1: %i ClockWiseMost: %i"),NewVertList(NewVertList.Num()-1),Poly1->PolyVerts(Poly1CCWMostIdx),Poly1->PolyVerts(Poly1ClockwiseMostIdx));
// 		for(INT Idx=0;Idx<Poly1->PolyVerts.Num();Idx++)
// 		{
// 			debugf(TEXT("POLY1 Vert(%i): %i"),Idx,Poly1->PolyVerts(Idx));
// 		}
// 		for(INT Idx=0;Idx<Poly2->PolyVerts.Num();Idx++)
// 		{
// 			debugf(TEXT("POLY2 Vert(%i): %i"),Idx,Poly2->PolyVerts(Idx));
// 		}
// 		for(INT Idx=0;Idx<NewVertList.Num();Idx++)
// 		{
// 			debugf(TEXT("NewVertList Vert(%i): %i"),Idx,NewVertList(Idx));
// 		}


		return NULL;
	}

	// find position in Poly2 of CCWMost vert from poly1
	VERTID Poly2VertPos = 0;
	for(INT Idx=0;Idx<Poly2->PolyVerts.Num();Idx++)
	{
		if(Poly2->PolyVerts(Idx) == Poly1->PolyVerts(Poly1CCWMostIdx))
		{
			Poly2VertPos=Idx;
		}
	}

	// now add verts from poly2 starting with CCWmost from poly1
	for(INT VertCount=0,Idx=Poly2VertPos;VertCount<Poly2->PolyVerts.Num();VertCount++,Idx++)
	{
		if(Idx>=Poly2->PolyVerts.Num())
		{
			Idx=0;
		}
		NewVertList.AddItem(Poly2->PolyVerts(Idx));
	}


	// remove unnecessary verts from the new vert list
	VERTID NextVert, PrevVert, CurVert= 0;

	TArray<FNavMeshPolyBase*> ExcludedPolys;
	ExcludedPolys.AddItem(Poly1);
	ExcludedPolys.AddItem(Poly2);

	TLookupMap<VERTID> VertsOnAnEdge_EndPointsExcluded;
	TLookupMap<VERTID> VertsOnAnEdge_EndPointsIncluded;
	
	FVector Norm = FNavMeshPolyBase::CalcNormal(NewVertList,this);

	for(INT Idx=NewVertList.Num()-1;Idx>=0;Idx--)
	{
		NextVert = (Idx+1>=NewVertList.Num()) ? NewVertList(0) : NewVertList(Idx+1);
		PrevVert = (Idx-1<0) ? NewVertList(NewVertList.Num()-1) : NewVertList(Idx-1); 
		CurVert = NewVertList(Idx);

		FVector Prev = GetVertLocation(PrevVert,LOCAL_SPACE)* AxisMask;
		FVector Current = GetVertLocation(CurVert,LOCAL_SPACE)* AxisMask;
		FVector Next = GetVertLocation(NextVert,LOCAL_SPACE)* AxisMask;

		FVector PrevAct = GetVertLocation(PrevVert,LOCAL_SPACE);
		FVector CurrentAct = GetVertLocation(CurVert,LOCAL_SPACE);
		FVector NextAct = GetVertLocation(NextVert,LOCAL_SPACE);


		// if there are two identical verts in the list, remove dupe
		if(CurVert == NextVert || CurVert == PrevVert)
		{
			NewVertList.Remove(Idx);
			Idx=NewVertList.Num();
			continue;
		}

		// don't remove vertex if it's used by any other poly
		const TArray<FNavMeshPolyBase*>& CurVertPolys = Verts(CurVert).ContainingPolys;
		if (CurVertPolys.Num() == 2)
		{
			if ((CurVertPolys(0) != Poly1 && CurVertPolys(0) != Poly2) ||
				(CurVertPolys(1) != Poly1 && CurVertPolys(1) != Poly2))
			{
				continue;
			}
		}
		else if (CurVertPolys.Num() > 2)
		{
			continue;
		}

		// if this vert is keeping the poly from being convex, and we can safely do so, remove it
		if(bDoEdgeSmoothing)
		{
			if( PerformEdgeSmoothingStep(NewVertList, ExcludedPolys,VertsOnAnEdge_EndPointsIncluded,VertsOnAnEdge_EndPointsExcluded, Idx,0.f,Norm,TRUE) ) 
			{
				NewVertList.Remove(Idx);
				Idx=NewVertList.Num();
				continue;
			}
		}
		else // only remove verts that are exactly colinear
		{
			FVector Closest(0.f);
			PointDistToSegment(CurrentAct,PrevAct,NextAct,Closest);

			FLOAT OffDist = aGetFlattenedDistanceBetweenVects(Closest,CurrentAct,Norm);
			// if it's a straight line just do it.
			if (OffDist < 0.01f)
			{
				NewVertList.Remove(Idx);
				Idx=NewVertList.Num();
				continue;
			}
		}


	}


	// if the new poly is still convex, remove the old and in with the new
	if(IsConvex(NewVertList,NAVMESHGEN_CONVEX_TOLERANCE))
	{
		UBOOL bControlPointsOK = TRUE;
		if(CtrlPtMap != NULL)
		{
			ControlPointList* CtrlPtList1 = CtrlPtMap->Find(Poly1);
			ControlPointList* CtrlPtList2 = CtrlPtMap->Find(Poly2);
			checkSlowish(CtrlPtList1 != NULL && CtrlPtList2 != NULL);

			Norm = FNavMeshPolyBase::CalcNormal(NewVertList,this);
			FVector Ctr = FNavMeshPolyBase::CalcCenter(NewVertList,this);

			bControlPointsOK = (VerifyNewPolyDistanceToControlPoints(Ctr,Norm,*CtrlPtList1,this) &&
								VerifyNewPolyDistanceToControlPoints(Ctr,Norm,*CtrlPtList2,this));
		}

		if(bControlPointsOK)
		{

			FVector NewBuildLoc = Poly1->PolyBuildLoc;

			ShiftVertsToCCWatIdxZero(NewVertList);

			RemovePoly(Poly1);
			RemovePoly(Poly2);


			// make sure the counter-clockwise-most vert is at index 0
			FNavMeshPolyBase* NewPoly = AddPolyFromVertIndices(NewVertList,NewPolyheight);
			NewPoly->PolyBuildLoc = NewBuildLoc;
			return NewPoly;
		}
	}

	return NULL;
}

/**
 * find a vertex in Poly that neighbors Vert, and is a border vertex
 * @return - MAXVERTID if none found
 */
VERTID FindBorderNeighboringVertex(UNavigationMeshBase* Mesh, VERTID Vert, TArray<VERTID>& VertsAlreadyQueued)
{

	FMeshVertex& OrigVert = Mesh->Verts(Vert);

	for(INT SharingIdx=0;SharingIdx<OrigVert.ContainingPolys.Num();SharingIdx++)
	{
		FNavMeshPolyBase* Poly = OrigVert.ContainingPolys(SharingIdx);
		INT Idx = 0;
		UBOOL bFound = Poly->PolyVerts.FindItem(Vert,Idx);
		checkSlowish(bFound);

		INT Next = (Idx+1 < Poly->PolyVerts.Num()) ? Idx+1 : 0;
		INT Prev = (Idx-1 >= 0) ? Idx-1 : Poly->PolyVerts.Num()-1;

		FMeshVertex& MidVert = Mesh->Verts(Vert);
		FMeshVertex& NextVert = Mesh->Verts( Poly->PolyVerts(Next) );
		FMeshVertex& PrevVert = Mesh->Verts( Poly->PolyVerts(Prev) );

		UBOOL bNextValid = (NextVert.ContainingPolys.Num() < 3 && !VertsAlreadyQueued.ContainsItem(Poly->PolyVerts(Next)));
		UBOOL bPrevValid = (PrevVert.ContainingPolys.Num() < 3 && !VertsAlreadyQueued.ContainsItem(Poly->PolyVerts(Prev)));
		if(bNextValid && bPrevValid)
		{
			// if they're both border verts, take the one with less sharing polys
			if(NextVert.ContainingPolys.Num() < PrevVert.ContainingPolys.Num())
			{
				return Poly->PolyVerts(Next);
			}

			return Poly->PolyVerts(Prev);
		}

		if(bNextValid)
		{		
			return Poly->PolyVerts(Next);
		}
		if(bPrevValid)
		{
			return Poly->PolyVerts(Prev);
		}
	}

	return MAXVERTID;	
}


/**
 * Determines if the passed vertex is part of an edge that the passed point is on top of
 * @param CenterVert - vert that we're trying to determine is on any edges of VertA or not
 * @param VertA - vert we're testing edges from
 * @param Mesh - mesh the verts live in
 * @return - TRUE If the vert is on one of the edges
 */
UBOOL VertHasEdgeThatPointIsOn(VERTID CenterVert, VERTID VertA, UNavigationMeshBase* Mesh )
{
	// all local space math
	FMeshVertex& Pt = Mesh->Verts(CenterVert);
	FMeshVertex& Vert = Mesh->Verts(VertA);

	for(INT ContainingPolyIdx=0;ContainingPolyIdx<Vert.ContainingPolys.Num();ContainingPolyIdx++)
	{
		FNavMeshPolyBase* Poly = Vert.ContainingPolys(ContainingPolyIdx);

		//for(INT VertIdx=0;VertIdx<Poly->PolyVerts.Num();VertIdx++)
		INT VertIdx=-1;
		if(Poly->PolyVerts.FindItem(VertA,VertIdx))
		{
			VERTID Next = (VertIdx+1<Poly->PolyVerts.Num()) ? Poly->PolyVerts(VertIdx +1) : Poly->PolyVerts(0);
			VERTID Prev = (VertIdx-1>=0) ? Poly->PolyVerts(VertIdx -1) : Poly->PolyVerts(Poly->PolyVerts.Num()-1);



			FVector Closest=FVector(0.f);
			FVector HeightlessPt   = FVector(Pt.X,Pt.Y,0.f);
			FVector HeightLessVert = FVector(Vert.X,Vert.Y,0.f);
			FVector HeightLessNext = FVector(Mesh->Verts(Next).X,Mesh->Verts(Next).Y,0.f);
			FVector HeightLessPrev = FVector(Mesh->Verts(Prev).X,Mesh->Verts(Prev).Y,0.f);

			// if they are the same edge, let it through
			if(Next == CenterVert || Prev == CenterVert)
			{
				return TRUE; 
			}

			// if the center vert is somewhere in the interior of the edge
			if(( PointDistToSegment(HeightlessPt,HeightLessVert,HeightLessNext,Closest) < KINDA_SMALL_NUMBER && !(Closest-HeightLessNext).IsNearlyZero())
				|| ( PointDistToSegment(HeightlessPt,HeightLessVert,HeightLessPrev,Closest) < KINDA_SMALL_NUMBER && !(Closest-HeightLessPrev).IsNearlyZero()))
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
 * Add a poly to the polygon octree for this navmesh
 * @param Poly - the poly to add
 */
void  UNavigationMeshBase::AddPolyToOctree(FNavMeshPolyBase* Poly)
{
	if(PolyOctree == NULL)
	{
		PolyOctree = new FPolyOctreeType(FVector(0.f),HALF_WORLD_MAX);
	}
	checkSlowish(PolyOctree != NULL);
	checkSlowish(!Poly->OctreeId.IsValidId());

	PolyOctree->AddElement(Poly);
	checkSlowish(Poly->OctreeId.IsValidId());
}

/**
 * removes a poly from the polygon octree for this mesh
 * @param Poly - the poly to remove
 */
void  UNavigationMeshBase::RemovePolyFromOctree(FNavMeshPolyBase* Poly)
{
	if( PolyOctree != NULL && Poly->OctreeId.IsValidId() )
	{
	checkSlowish(PolyOctree->GetElementById(Poly->OctreeId) == Poly);

	PolyOctree->RemoveElement(Poly->OctreeId);
	Poly->OctreeId = FOctreeElementId(); // clear Id
	checkSlowish(!Poly->OctreeId.IsValidId());
}
}

/**
 * determines if the passed FBox should be considered 'in' this poly (e.g. are we inside the poly's bounds, and close to its surface)
 * @param Box			- box to test
 * @param bWorldSpace	- what frame the incoming box i sin
 * @return UBOOL		- TRUE if this poly is valid for the passed box
 */
UBOOL FNavMeshPolyBase::ContainsBox( const FBox& Box, UBOOL bWorldSpace, FLOAT BoxPadding) const
{
	UBOOL bCloseEnoughToSurface = (BoxPadding < 0.f || FPlaneAABBIsect( GetPolyPlane(bWorldSpace), Box.ExpandBy(BoxPadding)));

	if(bCloseEnoughToSurface)
	{
 		return ContainsPoint(Box.GetCenter(),bWorldSpace);
 	}

	return FALSE;
}

/**
 * uses crossings test to determine of a point is within a poly
 * @param point		- the point to check
 * @param bWorldSpace - whether or not the incoming point is in local space or world space
 * @param Epsilon	- threshold value to use for comparison operations
 * @return UBOOL    - whether or not the passed point is within this polygon
 */
UBOOL FNavMeshPolyBase::ContainsPoint( const FVector& InPoint, UBOOL bWorldSpace, FLOAT Epsilon ) const
{
	// convert the incoming point to the proper space
	const FVector Point = (bWorldSpace && NavMesh != NULL) ? FVector(NavMesh->W2LTransformFVector(InPoint)) : InPoint;

	if(!BoxBounds.ExpandBy(Epsilon).IsInside(Point))
	{
// 		GWorld->GetWorldInfo()->DrawDebugBox(BoxBounds.GetCenter(),BoxBounds.GetExtent(),255,255,0,TRUE);
// 		GWorld->GetWorldInfo()->DrawDebugLine(BoxBounds.GetCenter(),InPoint,255,0,0,TRUE);
		return FALSE;
	}

	// if this poly is roughly flat, use fast 2D check
	if(Abs<FLOAT>(NavMesh->L2WTransformNormal(PolyNormal).Z)>= 0.5f)
	{
		if( Epsilon < KINDA_SMALL_NUMBER )
		{
			UBOOL bRet=FALSE;

			const FMeshVertex* RESTRICT MeshVertsPtr = (FMeshVertex*)NavMesh->Verts.GetData();
			const VERTID* RESTRICT PolyVertsPtr1 = (WORD*)PolyVerts.GetData();
			const VERTID* RESTRICT PolyVertsPtr2 = PolyVertsPtr1 + (PolyVerts.Num() - 1);

			for (INT Count = 0; Count < PolyVerts.Num(); Count++)
			{
				const FVector* RESTRICT PolyVertsPtrI = &MeshVertsPtr[*PolyVertsPtr1];
				const FVector* RESTRICT PolyVertsPtrJ = &MeshVertsPtr[*PolyVertsPtr2];

				if ( ((PolyVertsPtrI->Y > Point.Y) != (PolyVertsPtrJ->Y > Point.Y)) &&
					(Point.X < (PolyVertsPtrJ->X - PolyVertsPtrI->X) * (Point.Y - PolyVertsPtrI->Y) / (PolyVertsPtrJ->Y - PolyVertsPtrI->Y) + PolyVertsPtrI->X) )
				{
					bRet = !bRet;
				}
				PolyVertsPtr2 = PolyVertsPtr1++;
			}
			return bRet;
		}
		else
		{

			UBOOL bRet=FALSE;
			for (INT Idx = 0, Jdx = PolyVerts.Num()-1; Idx < PolyVerts.Num(); Jdx = Idx++)
			{
				FVector VertI = NavMesh->Verts(PolyVerts(Idx));
				VertI += (VertI - PolyCenter).SafeNormal()*Epsilon; // expand out so we include vertices/edges of original poly
				FVector VertJ = NavMesh->Verts(PolyVerts(Jdx));
				VertJ += (VertJ - PolyCenter).SafeNormal()*Epsilon; // expand out so we include vertices/edges of original poly

				if ( ((VertI.Y>Point.Y) != (VertJ.Y>Point.Y)) &&
					(Point.X < (VertJ.X-VertI.X) * (Point.Y-VertI.Y) / (VertJ.Y-VertI.Y) + VertI.X) )
				{
					bRet = !bRet;
				}
			}
			return bRet;
		}
	}
	else // if not, use slower 3D volumetric check
	{
		// MT->TODO: if there are two faces whose normals are facing away from each other
		//			 there is a gap in their 'contains point' space right on the boundary.. 
		//			 need to figure out how to decide which poly the point is contained by in these cases


		for(INT VertIdx=0;VertIdx<PolyVerts.Num();VertIdx++)
		{
			const FVector CurEdgePt0 = GetVertLocation(VertIdx,LOCAL_SPACE);
			const FVector CurEdgePt1 = GetVertLocation( ( VertIdx + 1 ) % PolyVerts.Num() ,LOCAL_SPACE);
			// find outward normal of this edge 
			const FVector CurEdgeNormal = ((CurEdgePt1 - CurEdgePt0) ^ PolyNormal).SafeNormal();

			FLOAT EdgeDot = ((Point - CurEdgePt0)|CurEdgeNormal);

			if(EdgeDot > KINDA_SMALL_NUMBER)
			{
				return FALSE;
			}
		}

		return TRUE;			
	}
}

/*
 * determines if the passed point is within this poly or not
 * STATIC version which takes a list of vectors as the poly we're checking against
 */
UBOOL FNavMeshPolyBase::ContainsPoint( const TArray<FVector>& PolyVerts, const FVector& Point )
{
	UBOOL bRet=FALSE;
	const FVector* RESTRICT PolyVertsPtrI = (FVector*)PolyVerts.GetData();
	const FVector* RESTRICT PolyVertsPtrJ = (FVector*)&PolyVerts(PolyVerts.Num() - 1);

	for (INT Count = 0; Count < PolyVerts.Num(); Count++)
	{
		if ( ((PolyVertsPtrI->Y > Point.Y) != (PolyVertsPtrJ->Y > Point.Y)) &&
			(Point.X < (PolyVertsPtrJ->X - PolyVertsPtrI->X) * (Point.Y - PolyVertsPtrI->Y) / (PolyVertsPtrJ->Y - PolyVertsPtrI->Y) + PolyVertsPtrI->X) )
		{
			bRet = !bRet;
		}
		PolyVertsPtrJ = PolyVertsPtrI++;
	}
	return bRet;
}
/**
 * GetPolySegmentSpanList
 * - Will search only polys in this mesh, and return a list of spans of polys which intersect the line segment given
 * @param Start - start of span
 * @param End   - end of span
 * @param out_Spans - out array of spans populated with intersecting poly spans
 */
void UNavigationMeshBase::GetPolySegmentSpanList(const FVector& Start, const FVector& End, TArray<FPolySegmentSpan>& out_Spans, UBOOL bWorldSpace/*=WORLD_SPACE*/,UBOOL bIgnoreDynamic/*=FALSE*/, UBOOL bReturnBothDynamicAndStatic/*=FALSE*/)
{
	FBox TestBounds(0);
	TestBounds += Start;
	TestBounds += End;
	TestBounds = TestBounds.ExpandBy(10.f);
	
	static TArray<FNavMeshPolyBase*> Polys;
	Polys.Reset();
	GetIntersectingPolys(TestBounds.GetCenter(),TestBounds.GetExtent(),Polys,bWorldSpace,bIgnoreDynamic,bReturnBothDynamicAndStatic);

	FNavMeshPolyBase* CurPoly = NULL;
	FVector EntryPoint(0.f),ExitPoint(0.f);

	for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
	{
		CurPoly = Polys(PolyIdx);
		if( CurPoly->IntersectsPoly2D(Start,End,EntryPoint,ExitPoint,bWorldSpace) )
		{
			out_Spans.AddItem(FPolySegmentSpan(CurPoly,EntryPoint,ExitPoint));
		}
	}
}

/**
 * ContainsPointOnBorder
 *  returns TRUE if the point passed is within the border polys of this mesh
 * @param Point - point to check for border-ness
 * @return TRUE if point is on border
 */	
UBOOL UNavigationMeshBase::ContainsPointOnBorder(const FVector& Point)
{
	if(PolyOctree != NULL)
	{
		FVector LS_Point = W2LTransformFVector(Point);
		FBoxCenterAndExtent QueryBox(LS_Point, FVector(1.f));

		// Iterate over the octree nodes containing the query point.
		for(FPolyOctreeType::TConstElementBoxIterator<> OctreeIt(*(PolyOctree),QueryBox); OctreeIt.HasPendingElements(); OctreeIt.Advance())
		{
			FNavMeshPolyBase* Poly = OctreeIt.GetCurrentElement();
			checkSlowish(Poly!=NULL);

			if(Poly->IsBorderPoly() && Poly->ContainsPoint(LS_Point))
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/** 
 * ContainsPoint
 * (( point should be in local space ))
 *  Tests whether the passed point is in this mesh
 * @param Point - point to test
 * @return TRUE if the point passed is within the border polys of this mesh
 */
UBOOL UNavigationMeshBase::ContainsPoint(const FVector& Point)
{
	static TArray<FNavMeshPolyBase*> Polys;
	Polys.Reset();
	GetIntersectingPolys(Point,FVector(5.f),Polys,LOCAL_SPACE,TRUE);

	for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
		{
		FNavMeshPolyBase* Poly = Polys(PolyIdx);

			if(Poly->ContainsPoint(Point))
			{
				return TRUE;
			}
		}
	return FALSE;
}

/**
 * returns TRUE if the AABB passed in intersects with the bounds of a poly in this mesh
 * @param Center - center of AABB
 * @param Extent - Extent of AABB
 * @param out_IntersecingPoly - out param filled with intersecting poly ( if any ) 
 * @param bWorldSpace - TRUE If the params are in world space
 * @return TRUE if there is an intersection
 */
UBOOL UNavigationMeshBase::IntersectsPolyBounds(const FVector& Center, const FVector& Extent, FNavMeshPolyBase*& out_IntersectingPoly, UBOOL bWorldSpace, DWORD TraceFlags/*=0*/ )
{
	static TArray<FNavMeshPolyBase*> Polys;
	Polys.Reset();
	GetIntersectingPolys(Center,Extent,Polys,bWorldSpace,TRUE,TraceFlags);

	for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
		{
		FNavMeshPolyBase* Poly = Polys(PolyIdx);
			out_IntersectingPoly=Poly;
			return TRUE;
		}


	return FALSE;
}

/**
 *	used for SAT in IntersectsPoly 
 *   will get the min/max distance on the passed axis
 * @param Points - points to generate min/max dist for
 * @param Direction - axis to test
 * @param out_Min - output param filled with min dist
 * @param out_Max - output param filled with max dist
 */
FORCEINLINE void GetMinMaxAfterProjection(const TArray<FVector>& Points, const FVector& Direction, FLOAT& out_Min, FLOAT& out_Max)
{
#if ENABLE_VECTORINTRINSICS
	// Load the direction
	VectorRegister VDirection = VectorLoadFloat3_W0(&Direction);

	VectorRegister VMin = VBigNumber;
	VectorRegister VMax = VNegBigNumber;

	const FVector* RESTRICT PointPtr = (FVector*)Points.GetData();

	// Find the min and max projection across all points
	for (INT Count = 0; Count < Points.Num(); Count++)
{
		// Load the point data into a register
		VectorRegister Point = VectorLoadFloat3_W0(PointPtr);
		// Calculate the dot product
		VectorRegister Dot = VectorDot3(Point,VDirection);
		// Do the Min/Max of each component
		VMin = VectorMin(Dot,VMin);
		VMax = VectorMax(Dot,VMax);
		PointPtr++;
	}

	// Store the results
	VectorStoreFloat1(VMin,&out_Min);
	VectorStoreFloat1(VMax,&out_Max);

#else
	out_Min = BIG_NUMBER;
	out_Max = -BIG_NUMBER;

	for(INT PointIdx=0;PointIdx<Points.Num();PointIdx++)
	{
		FLOAT Current = Points(PointIdx) | Direction;
		if(Current < out_Min)
		{
			out_Min = Current;
		}

		if(Current > out_Max)
		{
			out_Max = Current;
		}
	}
#endif
}

/**
 * uses SAT to determine if the passed set of points intersects this poly
 * @param OtherPolyVerts	-	Vertex locations representing poly to check collision against
 * @param bWorldSpace		-	What reference frame the incoming polys are in
 * @return whehter or not the passed poly collides with this one
 */
#define ISECT_EPSILON 0.01f
UBOOL FNavMeshPolyBase::IntersectsPoly(const TArray<FVector>& OtherPolyVerts,UBOOL bWorldSpace, const FLOAT ExpandDist)
{
	 
	// calc norm and ctr for incoming poly 
	FVector OtherPolyNorm = ((OtherPolyVerts(2) - OtherPolyVerts(1)) ^ (OtherPolyVerts(0) - OtherPolyVerts(1))).SafeNormal();

	// if the poly's plane does not clip this poly's AABB, throw it out
	FBox TransformedBBox = GetPolyBounds(bWorldSpace);
	FVector Ctr(0.f),Extent(0.f);
	TransformedBBox.GetCenterAndExtents(Ctr,Extent);
	if( !FPolyAABBIntersect(Ctr,Extent,OtherPolyVerts) )
	{
		return FALSE;
	}

	// throw all the points into an array for convenience
	TArray<FVector> ThisPolyVerts;
	for(INT ThisPolyVertIdx=0;ThisPolyVertIdx<PolyVerts.Num();ThisPolyVertIdx++)
	{
		FVector CurLoc = NavMesh->GetVertLocation(PolyVerts(ThisPolyVertIdx),bWorldSpace);
		if(ExpandDist > 0.f)
		{
			CurLoc += ((CurLoc - GetPolyCenter(bWorldSpace)).SafeNormal() * ExpandDist);
		}
		ThisPolyVerts.AddItem(CurLoc);
	}

	FVector Up = FVector(0.f,0.f,1.f);
	FVector ThisPolyNorm = GetPolyNormal(bWorldSpace);
	UBOOL bOtherPolyPerpToUp = Abs<FLOAT>((OtherPolyNorm | Up))  < ISECT_EPSILON;
	UBOOL bThisPolyPerpToUp = Abs<FLOAT>( ThisPolyNorm | Up) < ISECT_EPSILON;

	// *(Poly A)* loop through all the edges of this poly and generate a min/max projection for the current edge, then do the same for the other poly
	//       and check for overlap
	for(INT ThisPolyVertIdx=0;ThisPolyVertIdx<ThisPolyVerts.Num();ThisPolyVertIdx++)
	{
		FVector& ThisVert = ThisPolyVerts(ThisPolyVertIdx);
		FVector& ThisNextVert = ThisPolyVerts((ThisPolyVertIdx+1)%ThisPolyVerts.Num());
		FVector EdgeNorm = (ThisNextVert - ThisVert).SafeNormal() ^ (( bThisPolyPerpToUp ) ? ThisPolyNorm : FVector(0.f,0.f,1.f));

		FLOAT ThisMin, ThisMax;
		GetMinMaxAfterProjection(ThisPolyVerts,EdgeNorm,ThisMin,ThisMax);

		FLOAT OtherMin,OtherMax;
		GetMinMaxAfterProjection(OtherPolyVerts,EdgeNorm,OtherMin,OtherMax);

		// did we find a separating axis?
		//          >=                                             <=        (combat floating point treachery)
		if( (ThisMin - OtherMax) > -ISECT_EPSILON || (ThisMax - OtherMin) < ISECT_EPSILON)
		{
			// we did! NOT intersecting
			return FALSE;
		}
	}

	// *(Poly B)* loop through all the edges of this poly and generate a min/max projection for the current edge, then do the same for the other poly
	//       and check for overlap
	for(INT OtherPolyVertIdx=0;OtherPolyVertIdx<OtherPolyVerts.Num();OtherPolyVertIdx++)
	{
		const FVector& OtherVert = OtherPolyVerts(OtherPolyVertIdx);
		const FVector& OtherNextVert = OtherPolyVerts((OtherPolyVertIdx+1)%OtherPolyVerts.Num());
		FVector EdgeNorm = (OtherNextVert - OtherVert).SafeNormal() ^ ((bOtherPolyPerpToUp) ? OtherPolyNorm : FVector(0.f,0.f,1.f));

		FLOAT ThisMin, ThisMax;
		GetMinMaxAfterProjection(ThisPolyVerts,EdgeNorm,ThisMin,ThisMax);

		FLOAT OtherMin,OtherMax;
		GetMinMaxAfterProjection(OtherPolyVerts,EdgeNorm,OtherMin,OtherMax);


		// did we find a separating axis?
		//          >=											  <=      (combat floating point treachery)
		if((OtherMin - ThisMax) > -ISECT_EPSILON || (OtherMax - ThisMin) < ISECT_EPSILON)
		{
			// we did! NOT intersecting
			return FALSE;
		}
	}

	// could not find separating axis..
	return TRUE;

}

/**
* uses SAT to determine if the passed poly intersects this one
* @param Poly	-	the polygoin to check collision against
* @param bWorldSpace - should be TRUE if the incoming poly is in world space
* @param ExpandDist - fudge factor to expand the passed poly by 
* @return whehter or not the passed poly collides with this one
*/
UBOOL FNavMeshPolyBase::IntersectsPoly( FNavMeshPolyBase* OtherPoly, UBOOL bWorldSpace, const FLOAT ExpandDist)
{
	TArray<FVector> OtherPolyVerts;
	FVector CurLoc;

	if(ExpandDist > 0.f)
	{
		for( INT OtherPolyVertIdx = 0; OtherPolyVertIdx < OtherPoly->PolyVerts.Num(); ++OtherPolyVertIdx )
		{
			CurLoc = OtherPoly->NavMesh->GetVertLocation(OtherPoly->PolyVerts(OtherPolyVertIdx), bWorldSpace);
			CurLoc += ((CurLoc - OtherPoly->GetPolyCenter(bWorldSpace)).SafeNormal() * ExpandDist);
			OtherPolyVerts.AddItem(CurLoc);
		}
	}

	return IntersectsPoly(OtherPolyVerts, bWorldSpace, ExpandDist);
}

/**
 * static version of IntersectsPoly2D which takes an array of points rather than using this' polyverts
 * (assumes everything is in the same space)
 * @param Poly			 - array of points representing the poly
 * @param SegPt0		 - starting point for line segment
 * @param SegPt1		 - end point for line segment
 * @param out_EntryPoint - the point at which the line segment enters the polygon (if any)
 * @param out_ExitPoint  - the point at which the line segment leaves the polygon (if any)
 * @return				 - whether or not the segment intersects with this polygon
 */
UBOOL FNavMeshPolyBase::IntersectsPoly2D(const TArray<FVector>& Poly, const FVector& SegPt0, const FVector& SegPt1, FVector& out_EntryPoint, FVector& out_ExitPoint, FVector PolyNormal/*=FVector(0.f)*/)
{

	if((SegPt0-SegPt1).IsNearlyZero(0.01f))
	{
		out_ExitPoint=SegPt0;
		out_EntryPoint=SegPt0;
		return FNavMeshPolyBase::ContainsPoint(Poly,SegPt0);
	}

	if( PolyNormal.IsNearlyZero() )
	{
		PolyNormal = FNavMeshPolyBase::CalcNormal(Poly);
	}

	FLOAT T_Entry = 0.f;
	FLOAT T_Exit  = 1.0f;


	const FVector SegDir = SegPt1- SegPt0;


	for(INT VertIdx=0;VertIdx<Poly.Num();VertIdx++)
	{
		const FVector CurEdgePt0 = Poly(VertIdx);
		const FVector CurEdgePt1 = Poly(( VertIdx + 1 ) % Poly.Num());

		// find outward normal of this edge 
		const FVector CurEdgeNormal = ((CurEdgePt1 - CurEdgePt0) ^ PolyNormal).SafeNormal();

		const FVector Pt0ToPt0 = (SegPt0 - CurEdgePt0);
		FLOAT DotCurVert = -( Pt0ToPt0| CurEdgeNormal);
		FLOAT DotEdgeSeg = SegDir | CurEdgeNormal;

		// if the segment is parallel to this edge
		if(appIsNearlyZero(DotEdgeSeg,(FLOAT)KINDA_SMALL_NUMBER))
		{
			if(DotCurVert < -KINDA_SMALL_NUMBER)
			{
				// if this edge is parallel to the current edge, and the point is outside the poly then there is no intersection
				return FALSE;
			}
			else
			{
				//otherwise there may still be a collision, but it's not with this edge
				continue;
			}
		}

		FLOAT T = DotCurVert / DotEdgeSeg;


		if(DotEdgeSeg < 0.f)
		{
			T_Entry = Max<FLOAT>(T,T_Entry);
			//if(T_Entry > T_Exit)
			if((T_Entry - T_Exit) > KINDA_SMALL_NUMBER)
			{
				return FALSE;
			}
		}
		else if( DotEdgeSeg > 0.f)
		{
			T_Exit = Min<FLOAT>(T,T_Exit);
			//if(T_Exit < T_Entry)
			if((T_Exit - T_Entry) < -KINDA_SMALL_NUMBER)
			{
				return FALSE;
			}
		}		
	}

	// if we get this far then there is an intersection
	out_EntryPoint = SegPt0 + (SegDir * T_Entry);
	out_ExitPoint = SegPt0 + (SegDir * T_Exit);

	return TRUE;
}
/**
 * determines if the passed line segment intersects with this polygon (in 2D)
 * @param SegPt0		 - starting point for line segment
 * @param SegPt1		 - end point for line segment
 * @param out_EntryPoint - the point at which the line segment enters the polygon (if any)
 * @param out_ExitPoint  - the point at which the line segment leaves the polygon (if any)
 * @return				 - whether or not the segment intersects with this polygon
 */
UBOOL FNavMeshPolyBase::IntersectsPoly2D(const FVector& In_SegPt0, const FVector& In_SegPt1, FVector& out_EntryPoint, FVector& out_ExitPoint,UBOOL bWorldSpace/*=LOCAL_SPACE*/)
{
	// transform incoming stuff to local space
	FVector SegPt0(0.f),SegPt1(0.f);
	if( bWorldSpace)
	{	
		SegPt0 = NavMesh->W2LTransformFVector(In_SegPt0);
		SegPt1 = NavMesh->W2LTransformFVector(In_SegPt1);
	}
	else
	{
		SegPt0 = In_SegPt0;
		SegPt1 = In_SegPt1;
	}

	if(SegPt0.Equals(SegPt1,0.1f))
	{
		out_ExitPoint=SegPt0;
		out_EntryPoint=SegPt0;
		return ContainsPoint(In_SegPt0,bWorldSpace);
	}

	FVector Norm = PolyNormal;
#if !CONSOLE
	if( Norm.IsNearlyZero() )
	{
		Norm = CalcNormal(LOCAL_SPACE);
	}
#endif

	FLOAT T_Entry = 0.f;
	FLOAT T_Exit  = 1.0f;


	const FVector SegDir = SegPt1 - SegPt0;


	FVector CurEdgeNormal(0.f);
	const INT NumVerts = PolyVerts.Num();
	for(INT VertIdx=0;VertIdx<NumVerts;++VertIdx)
	{
		const FVector* RESTRICT CurEdgePt0 = &NavMesh->Verts(PolyVerts(VertIdx));
		const FVector* RESTRICT CurEdgePt1 = &NavMesh->Verts(PolyVerts(( VertIdx + 1 ) % PolyVerts.Num()));

		// find outward normal of this edge 
		CurEdgeNormal = ((*CurEdgePt1 - *CurEdgePt0) ^ Norm).SafeNormal();

		const FVector Pt0ToPt0 = (SegPt0 - *CurEdgePt0);
		FLOAT DotCurVert = -( Pt0ToPt0| CurEdgeNormal);
		FLOAT DotEdgeSeg = SegDir | CurEdgeNormal;

		// if the segment is parallel to this edge
		if(appIsNearlyZero(DotEdgeSeg,(FLOAT)KINDA_SMALL_NUMBER))
		{
			if(DotCurVert < -KINDA_SMALL_NUMBER)
			{
				// if this edge is parallel to the current edge, and the point is outside the poly then there is no intersection
				return FALSE;
			}
			else
			{
				//otherwise there may still be a collision, but it's not with this edge
				continue;
			}
		}

		FLOAT T = DotCurVert / DotEdgeSeg;

	
		if(DotEdgeSeg < 0.f)
		{
			T_Entry = Max<FLOAT>(T,T_Entry);
			//if(T_Entry > T_Exit)
			if((T_Entry - T_Exit) > KINDA_SMALL_NUMBER)
			{
				return FALSE;
			}
		}
		else if( DotEdgeSeg > 0.f)
		{
			T_Exit = Min<FLOAT>(T,T_Exit);
			//if(T_Exit < T_Entry)
			if((T_Exit - T_Entry) < -KINDA_SMALL_NUMBER)
			{
				return FALSE;
			}
		}		
	}

	// if we get this far then there is an intersection
	out_EntryPoint = SegPt0 + (SegDir * T_Entry);
	out_ExitPoint = SegPt0 + (SegDir * T_Exit);

	if( bWorldSpace )
	{
		out_EntryPoint = NavMesh->L2WTransformFVector(out_EntryPoint);
		out_ExitPoint = NavMesh->L2WTransformFVector(out_ExitPoint);
	}

	return TRUE;
}

/**
 * IntersectsPoly
 *  tests to see if the poly described by the passed vert locations intersects with this mesh
 * @param InPolyVertLocs - array of vert locations that describes the incoming polygon
 * @param out_IntersectingPoly - the poly in this mesh that intersects the passed poly (if any)
 * @param ExclusionPolys - polys we don't want to collide against
 * @param bWordlSpace - refernece frame of incoming vert locations
 * @param MinNormalZ - minimum Z for intersecting polys (e.g. polys steeper than this will be ignored)
 * @param TraceFlags - flags used for trace dilineation
 * @return TRUE if the poly described by the list of points intersects the mesh
 */
UBOOL UNavigationMeshBase::IntersectsPoly(TArray<FVector>& InPolyVertLocs,
											FNavMeshPolyBase*& out_IntersectingPoly,
											TArray<FNavMeshPolyBase*>* ExclusionPolys,
											UBOOL bWorldSpace,
											const FLOAT MinNormalZ/*=-1.f*/, 
											DWORD TraceFlags/*=0*/)
{
	FBox PolyBounds(0);
	for(INT Idx=0;Idx<InPolyVertLocs.Num();Idx++)
	{
		FVector LS_PolyVert = (bWorldSpace) ? FVector(W2LTransformFVector(InPolyVertLocs(Idx))) : InPolyVertLocs(Idx);
		PolyBounds+=LS_PolyVert+FVector(0.f,0.f,-10.f);
		PolyBounds+=LS_PolyVert;
		PolyBounds+=LS_PolyVert+FVector(0.f,0.f,+NAVMESHGEN_ENTITY_HALF_HEIGHT);
	}

	FBoxCenterAndExtent QueryBox(PolyBounds.GetCenter(), PolyBounds.GetExtent());

	// Iterate over the octree nodes containing the query point.

	static TArray<FNavMeshPolyBase*> Polys;
	Polys.Reset();
	GetIntersectingPolys(PolyBounds.GetCenter(),PolyBounds.GetExtent(),Polys,LOCAL_SPACE,TRUE,TraceFlags);

	for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
		{
		FNavMeshPolyBase* CurPoly = Polys(PolyIdx);
			if(ExclusionPolys != NULL && ExclusionPolys->ContainsItem(CurPoly))
			{
				continue;
			}

			if((MinNormalZ <= -1.f || CurPoly->GetPolyNormal().Z > MinNormalZ) && CurPoly->IntersectsPoly(InPolyVertLocs,bWorldSpace))
			{
				if(CurPoly->NumObstaclesAffectingThisPoly > 0)
				{
					checkSlowish(!CurPoly->GetObstacleInfo()->bNeedRecompute);

					UNavigationMeshBase* SubMesh = CurPoly->GetSubMesh();				
					checkSlowish(SubMesh != NULL);
					if( SubMesh->IntersectsPoly(InPolyVertLocs,out_IntersectingPoly,ExclusionPolys,bWorldSpace, MinNormalZ) )
					{
						return TRUE;
					}
				}
				else
				{
					out_IntersectingPoly=CurPoly;
					return TRUE;
				}
			}
		}

	out_IntersectingPoly=NULL;
	return FALSE;
}

/**
	* IntersectsPoly
	*  tests to see if the passed poly intersects with this mesh
	* @param Poly - array of vertIDs which describe the incoming poly 
	* @param TraceFlags - flas for trace dilineation 
	* @return TRUE if the poly described by the list of verts intersects the mesh
	*/
UBOOL UNavigationMeshBase::IntersectsPoly(TArray<VERTID>& InPoly, DWORD TraceFlags/*=0*/)
{
	FBox PolyBounds(0);
	TArray<FVector> PolyVertLocs;
	for(INT Idx=0;Idx<InPoly.Num();Idx++)
	{
		PolyVertLocs.AddItem(GetVertLocation(InPoly(Idx),LOCAL_SPACE));
	}

	FNavMeshPolyBase* Poly=NULL;
	return IntersectsPoly(PolyVertLocs,Poly,NULL,FALSE,TraceFlags);

}

void UNavigationMeshBase::FinishDestroy()
{
	Super::FinishDestroy();
	for(PolyList::TIterator It(BuildPolys.GetTail());It;)
	{	
		FNavMeshPolyBase* CurPoly = *It;
		--It;
		delete CurPoly;
	}

	if(PolyOctree != NULL)
	{
		delete PolyOctree;
		PolyOctree=NULL;
	}
	if( VertHash != NULL )
	{
		delete VertHash;
		VertHash = NULL;
	}

	FlushEdges();
}

void UNavigationMeshBase::BeginDestroy()
{
	Super::BeginDestroy();

	FNavMeshWorld* World = FNavMeshWorld::GetNavMeshWorld();

	if (World != NULL)
	{
		// this navmesh is being destroyed, remove references to polys in it from active obstacle map so we don't ref garbage data
		for( PolyObstacleInfoList::TIterator It(PolyObstacleInfoMap);It;++It)
		{
			FPolyObstacleInfo& Info = It.Value();

			for( INT ObstacleIdx=0; ObstacleIdx < Info.LinkedObstacles.Num(); ++ ObstacleIdx)
			{
				IInterface_NavMeshPathObstacle* LinkedObstacle = Info.LinkedObstacles(ObstacleIdx);
				World->ActiveObstacles.RemovePair(LinkedObstacle,FPolyReference(Info.Poly));
			}
		}
	}


	CleanupMeshReferences();
}

/**
 * returns TRUE if the vertex passed is an acute corner
 * @param Vert - vertID of the vert we want to test
 * @return TRUE if it is acute
 */
UBOOL UNavigationMeshBase::VertIsAcute( VERTID Vert )
{
	// find the shortest edge distance of any edge that uses this vert
	FVector TheVert = GetVertLocation(Vert,LOCAL_SPACE);

	FLOAT Min = 10.f;
	// move out diagonally from vert and see if each point is within the mesh
	INT NumInside = 0;
	FVector TestPoints[] = {TheVert + FVector(Min,Min,0.f),
		TheVert + FVector(-Min,Min,0.f),
		TheVert + FVector(-Min,-Min,0.f),
		TheVert + FVector(Min,-Min,0.f)};


	if(PolyOctree != NULL)
	{
		FBoxCenterAndExtent QueryBox(TheVert, FVector(Min));

		// Iterate over the octree nodes containing the query point.
		for(FPolyOctreeType::TConstElementBoxIterator<> OctreeIt(*(PolyOctree),QueryBox); OctreeIt.HasPendingElements(); OctreeIt.Advance())
		{
			FNavMeshPolyBase* Poly = OctreeIt.GetCurrentElement();
			checkSlowish(Poly!=NULL);

			for(INT Idx=0;Idx<4;Idx++)
			{
				if(Poly->ContainsPoint(TestPoints[Idx]))
				{
					NumInside++;
				}
			}
		}
	}



	// 	if(NumOutside==1)
	// 	{
	// 		GWorld->GetWorldInfo()->DrawDebugLine(TheVert,TestPoints[0],255,255,0,TRUE);
	// 		GWorld->GetWorldInfo()->DrawDebugLine(TheVert,TestPoints[1],255,255,0,TRUE);
	// 		GWorld->GetWorldInfo()->DrawDebugLine(TheVert,TestPoints[2],255,255,0,TRUE);
	// 		GWorld->GetWorldInfo()->DrawDebugLine(TheVert,TestPoints[3],255,255,0,TRUE);
	// 	}
	return(NumInside==3);
}

/**
 * GetAllPolysNearPoint
 *  will find all polygons which fall within the pass extent
 * @param Pt - (in LOCAL SPACE) center of extent
 * @param Extent - extent describing the box to check within
 * @param Result - output array that polys within the extent are copied to
 */
void UNavigationMeshBase::GetAllPolysNearPoint(const FVector& Pt, const FVector& Extent, TArray<FNavMeshPolyBase*>& Result)
{
	GetIntersectingPolys(Pt,Extent,Result,LOCAL_SPACE,TRUE);
}



/**
 * helper method which will add all verts in CurPoly that are within Box to the Result array
 * @param CurPoly - poly whose verts we're adding
 * @param Box - box we're checking against
 * @param Result - resulting list of vertex IDs
 */
void AddVertsWithinBox( FNavMeshPolyBase* CurPoly, FBox &Box, TArray<VERTID>& Result ) 
{
	for(INT PolyVertIdx=0;PolyVertIdx<CurPoly->PolyVerts.Num();++PolyVertIdx)
	{
		if( Box.IsInside(CurPoly->NavMesh->Verts(CurPoly->PolyVerts(PolyVertIdx))) )
		{
			Result.AddUniqueItem(CurPoly->PolyVerts(PolyVertIdx));
		}
	}
}

/**
 * GetAlLVertsNearPoint 
 *  returns all vertices within the passed extent centered about Pt
 * @param Pt - the center of the box to check
 * @Param Extent - extent describing the box to check within
 * @param Result - output array of vertids which fall within the box
 */
void UNavigationMeshBase::GetAllVertsNearPoint(const FVector& Pt, const FVector& Extent, TArray<VERTID>& Result)
{
	FBox Box = FBox::BuildAABB(Pt,Extent);
	if( KDOPInitialized )
	{
		
		static TArray<INT> TriIndices;
		TriIndices.Reset();

		FNavMeshCollisionDataProvider Prov(this,this,0);
		
		FBox Box = FBox::BuildAABB(Pt,Extent);
		TkDOPAABBQuery<FNavMeshCollisionDataProvider,WORD> kDOPQuery(Box,TriIndices,Prov, bNeedsTransform);
		KDOPTree.AABBQuery(kDOPQuery);

		for(INT Idx=0;Idx<TriIndices.Num();++Idx)
		{
			WORD PolyId = KDOPTree.Triangles(TriIndices(Idx)).MaterialIndex;
			FNavMeshPolyBase* CurPoly = &Polys(PolyId);
			AddVertsWithinBox(CurPoly,Box,Result);
		}

		return;
	}

	if( PolyOctree == NULL )
		return;


	FBoxCenterAndExtent QueryBox(Pt, Extent);
	

	// Iterate over the octree nodes containing the query point.

	// ->For each poly which intersects the incoming poly's bounds
	for(FPolyOctreeType::TConstElementBoxIterator<> OctreeIt(*(PolyOctree),QueryBox); OctreeIt.HasPendingElements(); OctreeIt.Advance())
	{
		FNavMeshPolyBase* CurPoly = OctreeIt.GetCurrentElement();
		AddVertsWithinBox(CurPoly,Box,Result);
	}
}

/**
 * GetPolyFromPoint
 *  Find the polygon which contains the passed point
 * @param Pt - the point to find a polygon for
 * @param Up - up vector of the point we're trying to find ( polys are volumetric, corner cases will have overlaps )
 * @param bWorldSpace - what ref frame the passed point is in
 * @return - the poly found that contains the point, NULL if none found
 */
FNavMeshPolyBase* UNavigationMeshBase::GetPolyFromPoint(const FVector& InPt, FLOAT MinWalkableZ, UBOOL bWorldSpace)
{
	FBox Box = FBox::BuildAABB(InPt,FVector(FVector(10.f,10.f,NAVMESHGEN_ENTITY_HALF_HEIGHT)));
	
	return GetPolyFromBox(Box,MinWalkableZ,bWorldSpace);
}


/**
 * this will take the passed poly and box and determine if there is a sub-poly that should be returned instead
 * @param ProposedPoly - poly we think the box is in
 * @param Box - box we're trying to determine the most appropriate poly for
 * @Param MinWalkableZ - minimum Z for normals walkable by the AI
 * @param bWorldSpace - TRUE if params are in world space
 * @return - the poly that should be used
 */
FNavMeshPolyBase* ArbitrateClosestPolyToBox(FNavMeshPolyBase* ProposedPoly, const FBox& Box, FLOAT MinWalkableZ, UBOOL bWorldSpace)
{
	// if the poly has obstacles, then we need to check the submesh 
	if(ProposedPoly->NumObstaclesAffectingThisPoly > 0)
	{
		FPolyObstacleInfo* Info = ProposedPoly->GetObstacleInfo();

		checkSlowish(Info->SubMesh != NULL);
		checkSlowish(!Info->bNeedRecompute);
		return Info->SubMesh->GetPolyFromBox(Box,MinWalkableZ,bWorldSpace);
	}

	return ProposedPoly;
}

/**
* GetPolyFromBox
*  Find the polygon which contains the box
* @param Box - the box we're checking for
* @param Up - up vector of the point we're trying to find ( polys are volumetric, corner cases will have overlaps )
* @param bWorldSpace - what ref frame the passed point is in
* @return - the poly found that contains the point, NULL if none found
*/
FNavMeshPolyBase* UNavigationMeshBase::GetPolyFromBox(const FBox& Box, FLOAT MinWalkableZ, UBOOL bWorldSpace)
{
	// Iterate over the octree nodes containing the query point.

	FVector LS_BoxCtr(0.f);

	FVector Box_Ctr(0.f), Box_Extent(0.f);
	Box.GetCenterAndExtents(Box_Ctr,Box_Extent);
	FBox LS_Box(0);

	static TArray<FNavMeshPolyBase*> Polys;
	Polys.Reset();
	if( KDOPInitialized )
	{
		GetIntersectingPolys(Box_Ctr,Box_Extent,Polys,bWorldSpace);

		if( bWorldSpace )
		{
			LS_BoxCtr = W2LTransformFVector(Box_Ctr);
		}
		else
		{
			LS_BoxCtr = Box_Ctr;
		}

		LS_Box = FBox::BuildAABB(LS_BoxCtr, Box_Extent );
	}
	else
	if(PolyOctree!=NULL)
	{	

		LS_BoxCtr = (bWorldSpace) ? FVector((W2LTransformFVector(Box_Ctr))) : Box_Ctr;

		LS_Box = FBox::BuildAABB(LS_BoxCtr, Box_Extent );

		FBoxCenterAndExtent QueryBox(LS_Box.ExpandBy(DEFAULT_BOX_PADDING));

		// ->For each poly which intersects the incoming poly's bounds
		for(FPolyOctreeType::TConstElementBoxIterator<> OctreeIt(*(PolyOctree),QueryBox); OctreeIt.HasPendingElements(); OctreeIt.Advance())
		{
			FNavMeshPolyBase* CurPoly = OctreeIt.GetCurrentElement();
			Polys.AddItem(CurPoly);	
		}
	}

	LS_Box = LS_Box.ExpandBy(1.f);

	TArray<FNavMeshPolyBase*> PolysContainingPoint;

	for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
	{
		FNavMeshPolyBase* CurPoly = Polys(PolyIdx);;

		FVector WS_Norm = CurPoly->GetPolyNormal(WORLD_SPACE);

		if(WS_Norm.Z >= MinWalkableZ-KINDA_SMALL_NUMBER)
		{
			// particularly tall or short objects can cause the center to exit the poly even though
			// the object is reachable so test Z extremes as well
			//@FIXME: should really be testing true intersection here
			FVector BoxCenter = LS_Box.GetCenter();
			if ( CurPoly->ContainsPoint(BoxCenter, LOCAL_SPACE) ||
				CurPoly->ContainsPoint(FVector(BoxCenter.X, BoxCenter.Y, LS_Box.Min.Z), LOCAL_SPACE) ||
				CurPoly->ContainsPoint(FVector(BoxCenter.X, BoxCenter.Y, LS_Box.Max.Z), LOCAL_SPACE) )
			{
				PolysContainingPoint.AddItem(CurPoly);
			}
		}

	}

	if( PolysContainingPoint.Num() > 0 )
	{
		// If there is only 1 intersecting poly, just return it
		if (PolysContainingPoint.Num() == 1)
		{
			return ArbitrateClosestPolyToBox(PolysContainingPoint(0),Box,MinWalkableZ,bWorldSpace);
		}

		// Find the intersecting poly with the normal closest matching Up
		FNavMeshPolyBase* BestPoly = NULL;
		FLOAT SignedDistFromPoly = 0.f;
		FLOAT BestDistFromPolySq = -1.1f;

		for(INT PolyIdx = 0; PolyIdx < PolysContainingPoint.Num(); ++PolyIdx)
		{
			FNavMeshPolyBase* PolyIt = PolysContainingPoint(PolyIdx);
			FVector Delta = LS_BoxCtr-PolyIt->GetClosestPointOnPoly(LS_Box.GetCenter(),LOCAL_SPACE);
			FLOAT DistanceSq = Delta.SizeSquared();

			if(DistanceSq < BestDistFromPolySq || BestDistFromPolySq < 0.f)
			{
				BestPoly = PolysContainingPoint(PolyIdx);
				BestDistFromPolySq = DistanceSq;
			}
		}

		checkSlowish(BestPoly != NULL);

		return ArbitrateClosestPolyToBox(BestPoly,Box,MinWalkableZ,bWorldSpace);
	}


	return NULL;
}

/**
 * GetPolyFromId
 *  Returns the polygon associated with the passed poly ID
 */
FNavMeshPolyBase* UNavigationMeshBase::GetPolyFromId(WORD Id)
{
	if( BuildPolyIndexMap.Num() == 0 )
	{
		if(Id >= Polys.Num())
		{
#if !CONSOLE
			if( GIsGame )
			{
				if(GWorld != NULL && GWorld->GetWorldInfo())
				{
					GWorld->GetWorldInfo()->bPathsRebuilt = FALSE;
				}
				warnf(NAME_Warning, TEXT("I just encountered a cross-pylon edge which references a polygon which no longer exists!  PATHS ARE OUT OF DATE!"));
			}
#endif
			return NULL;
		}
		return &Polys(Id);
	}
	else
	{
		return BuildPolyIndexMap(Id);
	}
}

/**
 * ShiftVertsToCCWatIdxZero
 *  Will shift around the vertlist until the vertex at 0 is top right of the poly
 * @param VertList - the list of verts to shift around
 */
void UNavigationMeshBase::ShiftVertsToCCWatIdxZero(TArray<VERTID>& VertList)
{

	// find center of poly
	FVector Ctr=FVector(0.f);
	for(INT Idx=0;Idx<VertList.Num();Idx++)
	{
		Ctr += GetVertLocation(VertList(Idx),LOCAL_SPACE);
	}
	Ctr /= VertList.Num();

	// find the top right vert
	INT New0Idx=-1;
	for(INT Idx=0;Idx<VertList.Num();Idx++)
	{
		INT NextIdx=(Idx+1) % VertList.Num();

		FVector Vert = GetVertLocation(VertList(Idx),LOCAL_SPACE);
		FVector NextVert = GetVertLocation(VertList(NextIdx),LOCAL_SPACE);

		if(Vert.Y < Ctr.Y && NextVert.Y >= Ctr.Y)
		{

			New0Idx=NextIdx;
			break;
		}
	}

	// now construct a new vert list starting with the top right
	if(New0Idx>=0)
	{
		TArray<VERTID> NewVertList;
		INT CurVertIdx=New0Idx;
		for(INT Idx=0;Idx<VertList.Num();Idx++)
		{
			NewVertList.AddItem(VertList(CurVertIdx));
			CurVertIdx=(CurVertIdx+1 < VertList.Num()) ? CurVertIdx+1 : 0;
		}
		VertList=NewVertList;	

	}

}

/**
 * constructs the KDOP tree for this mesh
 */
void AddPolyToKdopTriList(FNavMeshPolyBase* CurPoly, INT Idx, TArray<FkDOPBuildCollisionTriangle<WORD> >& KDOPTriangles)
{
	// MT-NOTE: KDOP tree wants things CCW, so add in reverse order since we're CW
	FMeshVertex& Vert0 = CurPoly->NavMesh->Verts(CurPoly->PolyVerts(0));
	for(INT VertIdx=CurPoly->PolyVerts.Num()-1;VertIdx>1;VertIdx--)
	{
		VERTID Vert1Idx = CurPoly->PolyVerts(VertIdx);
		VERTID Vert2Idx = CurPoly->PolyVerts(VertIdx-1);
		FMeshVertex& Vert1 = CurPoly->NavMesh->Verts(Vert1Idx);
		FMeshVertex& Vert2 = CurPoly->NavMesh->Verts(Vert2Idx);

		new (KDOPTriangles) FkDOPBuildCollisionTriangle<WORD>(CurPoly->PolyVerts(0), Vert1Idx, Vert2Idx,
			Idx,
			Vert0, Vert1, Vert2);
	}
}

/**
 * constructs the KDOP tree for this mesh
 * @param bFromBuildStructures (optional) - when TRUE the kdop will be constructed from build poly lists and not the normal 'polys' array
 */
void UNavigationMeshBase::BuildKDOP(UBOOL bFromBuildStructures)
{
	if(KDOPInitialized)
	{
		return;
	}

	KDOPInitialized=TRUE;
	FNavMeshPolyBase* CurPoly = NULL;
	
	static TArray<FkDOPBuildCollisionTriangle<WORD> > KDOPTriangles;
	KDOPTriangles.Reset();

	if( bFromBuildStructures )
	{
		// if we're creating kdop from build structures, use the right array, and also set up the index map so we can
		// ref back to the polys we're adding from
		INT Count = 0;
		BuildPolyIndexMap.Reset();
		for(PolyList::TIterator It(BuildPolys.GetHead());It;++It)
		{
			CurPoly = *It;
			// save ref to current index
			CurPoly->Item = Count;

			// TODO STEVEP SAMS - This will fix the case of the infinite loop due to the tri count going above MAXWORD (and the case of the invalid tri having an index of MAXWORD)
			// Consider changing the KDOP_IDX_TYPE to DWORD
			if(KDOPTriangles.Num() + CurPoly->PolyVerts.Num() >= MAXWORD)
			{
				break;
			}
			AddPolyToKdopTriList(CurPoly,Count++,KDOPTriangles);
			BuildPolyIndexMap.AddItem(CurPoly);
		}
	}
	else
	{
		for(INT Idx=0;Idx<Polys.Num();++Idx)
		{
			CurPoly = &Polys(Idx);
			// TODO STEVEP SAMS - This will fix the case of the infinite loop due to the tri count going above MAXWORD (and the case of the invalid tri having an index of MAXWORD)
			// Consider changing the KDOP_IDX_TYPE to DWORD
			if(KDOPTriangles.Num() + CurPoly->PolyVerts.Num() >= MAXWORD)
			{
				break;
			}
			AddPolyToKdopTriList(CurPoly,Idx,KDOPTriangles);	
		}
	}

	KDOPTree.Build(KDOPTriangles);
}

/**
 * constructs the KDOP tree for this mesh, forced update
 * @param bFromBuildStructures (optional) - when TRUE the kdop will be constructed from build poly lists and not the normal 'polys' array
 */
void UNavigationMeshBase::ForcedBuildKDOP(UBOOL bFromBuildStructures)
{
	KDOPInitialized = FALSE;
	BuildKDOP(bFromBuildStructures);
}

/**
 * Perform a line check against this mesh
 *
 * @param NormalNavigationMesh - the mesh of obstaclemesh/navigationmehs pairs which represents the navigation mesh
 * @param Result - output of the result of the linecheck
 * @param End - end point for linecheck
 * @param Start - start point for line check
 * @param Extent - the extent to use for the line/box check
 * @param TraceFlags - flags to use during the trace
 * @return TRUE if check did NOT hit anything
 */
UBOOL UNavigationMeshBase::LineCheck( UNavigationMeshBase* ForNavigationMesh, FCheckResult& Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags, FNavMeshPolyBase** out_HitPoly )
{
	checkSlowish(KDOPInitialized);
	if(!KDOPInitialized)
	{
		return TRUE;
	}


	UBOOL bDynamicHit = FALSE;
	FCheckResult DynamicResult(1.f);
	if( IsObstacleMesh() && GetPylon()->DynamicObstacleMesh != NULL )
	{
		bDynamicHit = !GetPylon()->DynamicObstacleMesh->LineCheck(ForNavigationMesh,DynamicResult,End,Start,Extent,TraceFlags,out_HitPoly);
	}


	FNavMeshCollisionDataProvider Prov(this,ForNavigationMesh,TraceFlags);

	if(!Extent.IsNearlyZero())
	{
		TkDOPBoxCollisionCheck<FNavMeshCollisionDataProvider,WORD> kDOPCheck(Start,End,Extent,TraceFlags,Prov,&Result);

		if(KDOPTree.BoxCheck(kDOPCheck))
		{
			Result.Normal = kDOPCheck.GetHitNormal();
			Result.Actor = ForNavigationMesh->GetPylon();
			Result.Component = NULL;
			Result.Time = Clamp(Result.Time - Clamp(  0.1f,0.1f / (End - Start).Size(), 4.0f / (End - Start).Size()  ),0.0f,1.0f);
			Result.Location = Start + (End - Start) * Result.Time;
			// extract the hit poly from the ItemID in the HitInfo (Note: if we hit a sub-poly, then this ptr shoudl be the right mesh, and everything will work

			// if we had a dynamic hit, return whichever result was first
			if( bDynamicHit && DynamicResult.Time < Result.Time )
			{
				Result = DynamicResult;
			}
			// if we have an outptr for hitpoly and we're not going with the dynamic hit, then grab the result poly
			else if(out_HitPoly != NULL)
			{
				*out_HitPoly = GetPolyFromId(Result.Item);
			}

			return FALSE;
		}
	}
	else
	{
		TkDOPLineCollisionCheck<FNavMeshCollisionDataProvider,WORD> kDOPCheck(Start,End,TraceFlags,Prov,&Result);

		if(KDOPTree.LineCheck(kDOPCheck))
		{
			Result.Normal = kDOPCheck.GetHitNormal();
			Result.Actor = ForNavigationMesh->GetPylon();
			Result.Component = NULL;
			Result.Time = Clamp(Result.Time - Clamp(  0.1f,0.1f / (End - Start).Size(), 4.0f / (End - Start).Size()  ),0.0f,1.0f);
			Result.Location = Start + (End - Start) * Result.Time;

			// if we had a dynamic hit, return whichever result was first
			if( bDynamicHit && DynamicResult.Time < Result.Time )
			{
				Result = DynamicResult;
			}
			// if we have an outptr for hitpoly and we're not going with the dynamic hit, then grab the result poly
			else if(out_HitPoly != NULL)
			{
				*out_HitPoly = GetPolyFromId(Result.Item);
			}

			return FALSE;
		}

	}

	if(bDynamicHit)
	{
		Result = DynamicResult;
		return FALSE;
	}

	return TRUE;
}

	
/**
 * Perform a point check against this mesh
 *
 * @param NormalNavigationMesh - the mesh of obstaclemesh/navigationmehs pairs which represents the navigation mesh
 * @param Result - output of the result of the linecheck
 * @param Pt - the point to check
 * @param Extent - the extent to use for the point check
 * @param TraceFlags - flags to use during the trace
 * @return TRUE if check did NOT hit anything
 */
UBOOL UNavigationMeshBase::PointCheck( class UNavigationMeshBase* ForNavigationMesh, FCheckResult& Result, const FVector& Pt, const FVector& Extent, DWORD TraceFlags, FNavMeshPolyBase** out_HitPoly )
{
	checkSlowish(KDOPInitialized);
	if(!KDOPInitialized)
	{
		return TRUE;
	}

	FNavMeshCollisionDataProvider Prov(this,ForNavigationMesh,TraceFlags);
	TkDOPPointCollisionCheck<FNavMeshCollisionDataProvider,WORD> kDOPCheck(Pt,Extent,Prov,&Result);

	if(KDOPTree.PointCheck(kDOPCheck))
	{
		Result.Normal = kDOPCheck.GetHitNormal();
		Result.Actor = ForNavigationMesh->GetPylon();
		Result.Component = NULL;
		Result.Time = 0.f;
		Result.Location = kDOPCheck.GetHitLocation();
		
		// extract the hit poly from the ItemID in the HitInfo (Note: if we hit a sub-poly, then this ptr shoudl be the right mesh, and everything will work
		if(out_HitPoly != NULL)
		{
			*out_HitPoly = GetPolyFromId(Result.Item);
		}
		return FALSE;
	}

	if( IsObstacleMesh() && GetPylon()->DynamicObstacleMesh != NULL )
	{
		return GetPylon()->DynamicObstacleMesh->PointCheck(ForNavigationMesh,Result,Pt,Extent,TraceFlags,out_HitPoly);
	}

	return TRUE;
}

/**
 * given an ideal cylinder location and radius, will fit that cylinder in this polygon as best it can and return the new position
 * @param CylIdealLoc - ideal location for cylinder
 * @param CylRadius   - radius of cylinder
 * @param CylHalfHeight - half the height of the cylinder
 * @param out_BestLoc - the best-fit location for the cylinder
 * @param bForceInteriorPosition - when FALSE, if the ideal position is within the poly, and not colliding with the obstacl mesh
                                   that position will be used.  Set to TRUE if you're looking for a position which allows the cylinder
								   to be fully within the polygon
* @return - whether or not a suitable position was found
*/
UBOOL FNavMeshPolyBase::GetBestLocationForCyl(const FVector& WS_CylIdealLoc, FLOAT CylRadius, FLOAT CylHalfHeight, FVector& out_BestLoc, UBOOL bForceInteriorPosition/*=FALSE*/)
{
#define BESTCYLLOCISECT_THRESH 5.0f

	FVector LS_CylIdealLoc = NavMesh->W2LTransformFVector(WS_CylIdealLoc);
	// first find the edge closest to the passed point
	FVector ClosestCornerPt(0.f);
	FLOAT BestCornerDist = BIG_NUMBER;

	FVector ClosestEdgePt(0.f);
	FLOAT ClosestEdgeDist = BIG_NUMBER;

	// if we don't care if the point is interior to the poly, check to see if it's non-colliding and return it if so
	if ( !bForceInteriorPosition && ContainsPoint( LS_CylIdealLoc,LOCAL_SPACE ) )
	{	
		// if the point is already in the poly, and we aren't colliding then run with it
		FCheckResult Hit(1.f);
		if ( UNavigationHandle::StaticObstaclePointCheck(Hit,WS_CylIdealLoc,FVector(CylRadius)))
		{
			out_BestLoc = WS_CylIdealLoc;
			return TRUE;
		}
	}

	check(PolyVerts.Num() > 2);
	FVector PrevVert = NavMesh->GetVertLocation(PolyVerts(PolyVerts.Num()-1),LOCAL_SPACE);
	FVector Vert = NavMesh->GetVertLocation(PolyVerts(0),LOCAL_SPACE);
	FVector NextVert = NavMesh->GetVertLocation(PolyVerts(1),LOCAL_SPACE);

	FLOAT OffsetDist = CylRadius * 1.414f; // 1.414f to account for box check inaccuracy sqrt(2)
	for(INT VertIdx=1;VertIdx<=PolyVerts.Num();VertIdx++)
	{
		const FVector NextSeg = (NextVert-Vert);
		const FVector PrevSeg = (PrevVert-Vert);

		const FVector CornerUp = (NextSeg ^ PrevSeg).SafeNormal();

		// if this corner's UP is not up, it's probably two segments that are close to colinear
		// so skip it
		if( (CornerUp | PolyNormal) < 0.6f )
		{
			continue;
		}
		const FVector PrevPerp = (PrevSeg ^ CornerUp).SafeNormal();
		const FVector NextPerp = (CornerUp ^ NextSeg).SafeNormal();
		
		// project both segments in by entity width	
		const FVector NextOffset = NextPerp * OffsetDist;
		const FVector PrevOffset = PrevPerp * OffsetDist;

		// check for closest corner pt
		FVector Closest1(0.f),Closest2(0.f);
		SegmentDistToSegmentSafe(NextVert+NextOffset,Vert+NextOffset,PrevVert+PrevOffset,Vert+PrevOffset,Closest1,Closest2);
		FLOAT Dist = (Closest1-Closest2).SizeSquared();
		if (Dist < BESTCYLLOCISECT_THRESH)
		{
			
			const FVector AvgPt = Closest1;//they're close so just use one of them

			if( ContainsPoint(AvgPt,LOCAL_SPACE) )
			{
				FLOAT CurDist = (AvgPt-LS_CylIdealLoc).SizeSquared();
				if(CurDist < BestCornerDist)
				{
					BestCornerDist = CurDist;
					ClosestCornerPt=AvgPt;
				}
			}
		}

		// check for closest point on the edge
		Dist = PointDistToLine(LS_CylIdealLoc,NextSeg,Vert,Closest1);
		if( Dist < ClosestEdgeDist )
		{
			ClosestEdgeDist = Dist;
			ClosestEdgePt = Closest1+NextOffset;
		}



		PrevVert = Vert;
		Vert = NextVert;
		NextVert = NavMesh->GetVertLocation(PolyVerts( (VertIdx+1) % PolyVerts.Num()),LOCAL_SPACE);
	}

	// try to come in from the closest edge first, if it's in the poly still.. use it!
	if( ContainsPoint( ClosestEdgePt, LOCAL_SPACE) )
	{
		AdjustPositionToDesiredHeightAbovePoly(ClosestEdgePt,CylHalfHeight,LOCAL_SPACE);
		out_BestLoc = NavMesh->L2WTransformFVector( ClosestEdgePt );
		return TRUE;
	}

	// if we found a valid point, use it!
	if( BestCornerDist < BIG_NUMBER )
	{

		AdjustPositionToDesiredHeightAbovePoly(ClosestCornerPt,CylHalfHeight,LOCAL_SPACE);
		out_BestLoc = NavMesh->L2WTransformFVector( ClosestCornerPt );
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns the closest point to TestPt that is on this polygon
 * Note: is not quite the same as canonical closestptonpoly because our definition of 'in' is that of
 *       of a volume described by planes at the edges of the polygon, going in the direction of the edge normal, not the poly normal
 * @param TestPt - point to find the closest point on this poly to
 * @param bWorldSpace - the vector space the incoming point is in (and that the outgoing point should be in)
 * @return the closest point
 */
FVector FNavMeshPolyBase::GetClosestPointOnPoly( const FVector& TestPt, UBOOL bWorldSpace )
{
	// find the closest point on the plane of this poly
	const FVector Closest_Plane_Pt = FPointPlaneProject(TestPt,GetPolyCenter(bWorldSpace),GetPolyNormal(bWorldSpace));

	// if the closest point on the plane is still within the poly return that
	if( ContainsPoint( Closest_Plane_Pt, LOCAL_SPACE, 0.f ) )
	{
		return Closest_Plane_Pt;
	}

	// Otherwise, Loop through the edges and find edge with nearest point
	FLOAT BestDist = -1;
	FVector BestPt = FVector(0.f);
	
	for(INT VertIdx=0;VertIdx<PolyVerts.Num();++VertIdx)
	{
		const FVector CurVert = GetVertLocation(VertIdx,bWorldSpace);
		const FVector NextVert = GetVertLocation( (VertIdx+1)%PolyVerts.Num() ,bWorldSpace);

		FVector ClosestPtToSeg(0.f);
		FLOAT CurDist = PointDistToSegment(Closest_Plane_Pt,CurVert,NextVert,ClosestPtToSeg);

		if(CurDist < BestDist || BestDist < 0.f)
		{
			BestDist = CurDist;
			BestPt = ClosestPtToSeg;
		}
	}

	return BestPt;
}

/** 
 * Finds the pt on a given segment that crosses an boundary of this poly 
 * @param S1 - point one of segment
 * @Param S2 - point two of segment
 * @param out_Intersection - out param filled with intersection point
 * @return TRUE if there was an intersection 
 */
UBOOL FNavMeshPolyBase::GetBoundaryIntersection( FVector& S1, FVector& S2, FVector& out_Intersection )
{
	for( INT VertIdx = 0; VertIdx < PolyVerts.Num(); VertIdx++ )
	{
		INT NextVertIdx = (VertIdx+1)%PolyVerts.Num();

		FVector PolyPt(0.f), EdgePt(0.f);
		SegmentDistToSegmentSafe( GetVertLocation(VertIdx), GetVertLocation(NextVertIdx), S1, S2, PolyPt, EdgePt );
		if( (PolyPt-EdgePt).SizeSquared2D() < KINDA_SMALL_NUMBER*KINDA_SMALL_NUMBER )
		{
			out_Intersection = EdgePt;
			return TRUE;
		}
	}

	return FALSE;
}

/** 
 * Gets vert location for given local index 
 * @param LocalVertIdx - local index of vert you want location of
 * @param bWOrldSpace - TRUE if you want the location in world space
 * @return the location of the vert
 */
FVector FNavMeshPolyBase::GetVertLocation( INT LocalVertIdx, UBOOL bWorldSpace ) const
{
	return NavMesh->GetVertLocation(PolyVerts(LocalVertIdx),bWorldSpace);
}

/**
 * finds an edge in this poly's edge list which points back to the destination poly
 * @param DestPoly - Poly to find an edge to
 * @param bAllowTopLevelEdgesWhenSubMeshPresent - whether or not we want to allow top level edges that link to polys with obstacles 
 * @return - the edge found NULL of none was found
 */
FNavMeshEdgeBase* FNavMeshPolyBase::GetEdgeTo( FNavMeshPolyBase* DestPoly, UBOOL bAllowTopLevelEdgesWhenSubMeshPresent )
{
	INT NumEdges = GetNumEdges();
	for(INT EdgeIdx=0;EdgeIdx<NumEdges;EdgeIdx++)
	{
		FNavMeshEdgeBase* Edge = GetEdgeFromIdx(EdgeIdx,NULL,bAllowTopLevelEdgesWhenSubMeshPresent);
		if(Edge != NULL && (Edge->GetPoly0() == DestPoly || Edge->GetPoly1() == DestPoly))
		{
			return Edge;
		}
	}

	return NULL;
}


/**
 * Sets this polygon as a border (or exploration boundary) polygon, and maintains adjacent border-ness
 *	@param bBorderPoly - TRUE if this should be marked as a border poly
 *  @param AdjacentPolys - list of polys adjacent to this one
 */
void FNavMeshPolyBase::SetBorderPoly(UBOOL bBorderPoly, TArray<FNavMeshPolyBase*>* AdjacentPolys)
{
	if( GIsGame )
	{
		return; 
	}

	if(bBorderPoly && BorderListNode == NULL)
	{
		NavMesh->BorderPolys.AddHead(this);
		BorderListNode = NavMesh->BorderPolys.GetHead();
	}
	else if(!bBorderPoly && BorderListNode!=NULL)
	{
		NavMesh->BorderPolys.RemoveNode(BorderListNode);
		BorderListNode=NULL;
	}

	if(AdjacentPolys != NULL)
	{
		// ** loop through adjacent polys and maintain their border-ness 
		for(INT Idx=0;Idx<AdjacentPolys->Num();Idx++)
		{
			(*AdjacentPolys)(Idx)->SetBorderPoly((*AdjacentPolys)(Idx)->IsBorderPoly());
		}
	}
}

/**
 * takes an input list of vert IDs and calculates the area of the polygon they represent
 * @param Verts - vert IDs to calc area for
 * @param NavMesh - the navigation mesh these vert IDs are from
 * @return - the area of the polygon
 */
FLOAT FNavMeshPolyBase::CalcArea(const TArray<VERTID>& PolyVerts, UNavigationMeshBase* NavMesh)
{
	if(PolyVerts.Num() < 3)
	{
		//debugf(TEXT("WARNING: Degenerate poly (<3 verts) detected from CalcArea, returning 0 Area"));
		return 0.f;
	}	


	FVector A = NavMesh->GetVertLocation(PolyVerts(0),LOCAL_SPACE);

	FLOAT Area = 0.f;
	for(INT VertIdx=1; VertIdx<PolyVerts.Num()-1;VertIdx++)
	{
		FVector B = NavMesh->GetVertLocation(PolyVerts(VertIdx),LOCAL_SPACE);
		FVector C = NavMesh->GetVertLocation(PolyVerts(VertIdx+1),LOCAL_SPACE);

		Area += Abs<FLOAT>(TriangleArea2_3D(A,B,C)*0.5f);
	}

	return Area;

}

/**
* takes an input list of vert locations and calculates the area of the polygon they represent
* @param Verts - the vert locations to calculate area for
* @return - the area of the polygon
*/
FLOAT FNavMeshPolyBase::CalcArea(const TArray<FVector>& PolyVerts)
{
	//checkSlowish(PolyVerts.Num() >= 3);
	if(PolyVerts.Num() < 3)
	{
		debugf(TEXT("WARNING: Degenerate poly (<3 verts) detected from CalcArea, returning 0 Area"));
		return 0.f;
	}	


	FVector A = PolyVerts(0);

	FLOAT Area = 0.f;
	for(INT VertIdx=1; VertIdx<PolyVerts.Num()-1;VertIdx++)
	{
		FVector B = PolyVerts(VertIdx);
		FVector C = PolyVerts(VertIdx+1);

		Area += Abs<FLOAT>(TriangleArea2_3D(A,B,C)*0.5f);
	}

	return Area;

}

/**
* takes an input list of vert locations and calculates the area of the polygon they represent
* @param Verts - the vert locations to calculate area for
* @return - the area of the polygon
*/
FLOAT FNavMeshPolyBase::CalcArea()
{
	return CalcArea(PolyVerts,NavMesh);
}

/**
 * given a list of vertIDs..
 * calculates the normal for this poly based on its verts
 * @param Verts - the array of vertIDs to use to calculate the normal
 * @param NavMesh - the navmesh which owns the verts we hare getting IDs for
 * @param bWorldSpace - what frame of reference the poly should be returned in
 * @return - the calculated normal
 */
FVector FNavMeshPolyBase::CalcNormal(const TArray<VERTID>& PolyVerts, UNavigationMeshBase* NavMesh, UBOOL bWorldSpace)
{
	FVector Normal = FVector(0.f);

	if( bWorldSpace )
	{
		for(INT VertIdx=0;VertIdx<PolyVerts.Num();++VertIdx)
		{
			const FVector CurVert = NavMesh->GetVertLocation(PolyVerts(VertIdx),bWorldSpace);
			const FVector NextVert = NavMesh->GetVertLocation(PolyVerts((VertIdx+1) % PolyVerts.Num() ),bWorldSpace);
			Normal.X += (CurVert.Y - NextVert.Y) * (CurVert.Z + NextVert.Z);
			Normal.Y += (CurVert.Z - NextVert.Z) * (CurVert.X + NextVert.X);
			Normal.Z += (CurVert.X - NextVert.X) * (CurVert.Y + NextVert.Y);
		}
	}
	else // LOCAL SPACE, use data directly
	{
		if( PolyVerts.Num() < 3 )
		{
			return Normal;
		}
		
		const FMeshVertex* RESTRICT MeshVertsPtr = (FMeshVertex*)NavMesh->Verts.GetData();
		const VERTID* RESTRICT PolyVertsPtr1 = (WORD*)PolyVerts.GetData();
		const VERTID* RESTRICT PolyVertsPtr2 = (WORD*)PolyVerts.GetData();
		++PolyVertsPtr2;


		for(INT VertIdx=0;VertIdx<PolyVerts.Num();++VertIdx)
		{
			const FVector* RESTRICT CurVert = &MeshVertsPtr[*PolyVertsPtr1];
			const FVector* RESTRICT NextVert = &MeshVertsPtr[*PolyVertsPtr2];

			Normal.X += (CurVert->Y - NextVert->Y) * (CurVert->Z + NextVert->Z);
			Normal.Y += (CurVert->Z - NextVert->Z) * (CurVert->X + NextVert->X);
			Normal.Z += (CurVert->X - NextVert->X) * (CurVert->Y + NextVert->Y);

			PolyVertsPtr1++;
			if( VertIdx >= PolyVerts.Num()-2)
			{
				PolyVertsPtr2 = (WORD*)PolyVerts.GetData();
			}
			else
			{
				PolyVertsPtr2++;
			}
		}
	}

	return Normal.SafeNormal();
}

/**
 * calculate normal of the given polygon based on world space vertex locations
 * @param VertLocs - Locations of verts to use to calc normal
 * @return - normal of poly
 */
FVector FNavMeshPolyBase::CalcNormal(const TArray<FVector>& VertLocs)
{
	FVector Normal = FVector(0.f);

	for(INT VertIdx=0;VertIdx<VertLocs.Num();++VertIdx)
	{
		const FVector CurVert = VertLocs(VertIdx);
		const FVector NextVert = VertLocs( (VertIdx+1) % VertLocs.Num() );
		Normal.X += (CurVert.Y - NextVert.Y) * (CurVert.Z + NextVert.Z);
		Normal.Y += (CurVert.Z - NextVert.Z) * (CurVert.X + NextVert.X);
		Normal.Z += (CurVert.X - NextVert.X) * (CurVert.Y + NextVert.Y);
	}

	return Normal.SafeNormal();

}

/**
 * calculates the normal for this poly based on its verts
 * @param bWorldSpace - what frame of reference the poly should be returned in
 * @return - the calculated normal
 */
FVector FNavMeshPolyBase::CalcNormal(UBOOL bWorldSpace)
{
	return CalcNormal(PolyVerts,NavMesh,bWorldSpace);
}

/**
 *	GetPolyCenter get poly centroid
 * @param bWorldSpace - TRUE if you want the centroid in world space
 * @return - the poly centroid
 */
FVector FNavMeshPolyBase::GetPolyCenter(UBOOL bWorldSpace/* =WORLD_SPACE */) const
{
	return (bWorldSpace && NavMesh != NULL) ? FVector(NavMesh->L2WTransformFVector(PolyCenter)) : PolyCenter;
}

/**
 *	Returns the cached normal of this poly
 * @param bWorldSpace - true if you want the normal in world space
 * @return the normal
 */
FVector FNavMeshPolyBase::GetPolyNormal(UBOOL bWorldSpace/* =WORLD_SPACE */) const
{
	return (bWorldSpace && NavMesh != NULL) ? FVector(NavMesh->L2WTransformNormal(PolyNormal)) : PolyNormal;
}

/**
 *	return the bounds of this polygon 
 * @param bWorldSpace - TRUE if you want the bounds in world space
 * @return FBox representing the bounds of htis poly
 */
FBox FNavMeshPolyBase::GetPolyBounds(UBOOL bWorldSpace/*=WORLD_SPACE*/) const
{
	return (bWorldSpace && NavMesh != NULL && NavMesh->bNeedsTransform) ? BoxBounds.TransformBy(NavMesh->LocalToWorld) : BoxBounds;
}

/**
 *	calculate the centroid of this polygon and return it
 * @param bWorldSpace
 * @return the calc'd centroid 
 */
FVector FNavMeshPolyBase::CalcCenter(UBOOL bWorldSpace)
{
	return CalcCenter(PolyVerts,NavMesh,bWorldSpace);
}

/**
 *	Calculate the centroid of the polygon represented by the passed VERTIDs/mesh
 * @param Verts - IDs of verts to use for calc
 * @param NavMesh - mesh where the verts live
 * @param bWorldSpace - TRUE if center should be in  world space
 */
FVector FNavMeshPolyBase::CalcCenter(const TArray<VERTID>& Verts, UNavigationMeshBase* NavMesh,UBOOL bWorldSpace)
{
	FVector Ctr = FVector(0.f);
	for(INT VertIdx=0;VertIdx<Verts.Num();VertIdx++)
	{
		Ctr += NavMesh->GetVertLocation(Verts(VertIdx),bWorldSpace);
	}
	Ctr /= Verts.Num();

	return Ctr;
}

/**
 *	@return TRUE if this is a "border" poly (e.g. a poly on the boundary of exploration)
 */
UBOOL FNavMeshPolyBase::IsBorderPoly()
{
	// don't care about border polys except during offline (editor) builds
	if( GIsGame )
	{
		return FALSE;
	}

	// if we have any polys which are shared by 2 or less polys then we are on the border
	for(INT VertIdx=0;VertIdx<PolyVerts.Num();VertIdx++)
	{
		if(NavMesh->Verts(PolyVerts(VertIdx)).IsBorderVert(PolyVerts(VertIdx)))
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 *	returns a list of polys adjacent to this one which are also border polys
 * @param OutPolys - out param filled with adjacent border polys
 */
void FNavMeshPolyBase::GetAdjacentBorderPolys(TArray<FNavMeshPolyBase*>& OutPolys)
{
	if( GIsGame )
	{
		return;
	}

	for(INT VertIdx=0;VertIdx<PolyVerts.Num();VertIdx++)
	{
		VERTID VertID = PolyVerts(VertIdx);
		FMeshVertex& Vert = NavMesh->Verts(VertID);
		if(Vert.IsBorderVert(VertID))
		{
			for(INT SharerIdx=0;SharerIdx<Vert.ContainingPolys.Num();SharerIdx++)
			{
				if(Vert.ContainingPolys(SharerIdx) != this && Vert.ContainingPolys(SharerIdx)->IsBorderPoly())
				{
					OutPolys.AddUniqueItem(Vert.ContainingPolys(SharerIdx));
				}
			}
		}
	}
}

/**
 *	returns a list of polys adjacent to this one (only valid during builds)
 * @param OutPolys - out param filled with adjacent polys
 */
void FNavMeshPolyBase::GetAdjacentPolys(TArray<FNavMeshPolyBase*>& OutPolys)
{
	TLookupMap<FNavMeshPolyBase*> AdjacentPolys;
	
	if( GetNumEdges() == 0)
	{
		// using vert containing info first
		for(INT VertIdx=0;VertIdx<PolyVerts.Num();VertIdx++)
		{
			FMeshVertex& Vert = NavMesh->Verts(PolyVerts(VertIdx));

			if(Vert.ContainingPolys.Num() > 0 )
			{
				for(INT SharerIdx=0;SharerIdx<Vert.ContainingPolys.Num();SharerIdx++)
				{
					if(Vert.ContainingPolys(SharerIdx) != this)
					{
						AdjacentPolys.AddItem(Vert.ContainingPolys(SharerIdx));
					}
				}
			}
			else
			{
				for(INT SharerIdx=0;SharerIdx<Vert.PolyIndices.Num();SharerIdx++)
				{
					FNavMeshPolyBase* SharingPoly =  NavMesh->GetPolyFromId(Vert.PolyIndices(SharerIdx));
					if(SharingPoly != this)
					{
						AdjacentPolys.AddItem(SharingPoly);
					}
				}
			}
		}
	}
	else
	// now from edges
	if(!NavMesh->IsObstacleMesh())
	{
		for(INT EdgeIdx=0;EdgeIdx<GetNumEdges();++EdgeIdx)
		{
			FNavMeshEdgeBase* CurEdge = GetEdgeFromIdx(EdgeIdx);
			if(CurEdge != NULL)
			{
				FNavMeshPolyBase* OtherPoly = CurEdge->GetOtherPoly(this);

				if(OtherPoly != NULL)
				{
					AdjacentPolys.AddItem(OtherPoly);
				}
			}
		}
	}

	AdjacentPolys.GenerateKeyArray(OutPolys);
}

/** 
 * Get local index into PolyVerts adjacent to given local index 
 * @param LocalVertIdx - index in poly's vert array of vertex we're interested in
 * @param Dir - direction to get adjacent vert in (forward/bwd)
 * @return local index adjacent to the passed local index in the direction specified
 */
INT FNavMeshPolyBase::GetAdjacentVertexIndex( INT LocalVertIdx, INT Dir )
{
	if( Dir > 0 )
	{
		return ((LocalVertIdx+Dir)%PolyVerts.Num());
	}
	else
	{
		return ((LocalVertIdx+PolyVerts.Num()+Dir)%PolyVerts.Num());		
	}
}

/** 
 * Get vertex pool index adjacent to given local index 
 * @param LocalVertIdx - index in poly's vert array of vertex we're interested in
 * @param Dir - direction to get adjacent vert in (forward/bwd)
 * @return - mesh's vertex ID of adjacent vert
 */
VERTID FNavMeshPolyBase::GetAdjacentVertPoolIndex( INT LocalVertIdx, INT Dir )
{
	return PolyVerts(GetAdjacentVertexIndex(LocalVertIdx,Dir));
}

/** @returns the number of edges this poly has (including cross pylon edges) */
INT FNavMeshPolyBase::GetNumEdges()
{
	INT DynamicEdges = 0;
	if(NavMesh != NULL)
	{
		DynamicEdges = NavMesh->DynamicEdges.Num(Item);
	}
	return PolyEdges.Num()+DynamicEdges;
}

/**
 * @@WARNING! - this may return NULL if the edge you requested is currently invalid 
 * returns the edge at the given index (Cross Pylon edges start after the last normal edge idx)
 * @param Idx - local (to this poly) index of the edge requested
 * @param MeshOverride - Mesh to grab edges from, using this poly's index info (used to get linked edges from the obstacle mesh)
 *                       default indicates to use this poly's mesh
 * @param bAllowTopLevelEdgesWhenSubMeshPresent - when TRUE edges which have been superceded by submesh edges will still be returned 
 * @return - the Edge requested
*/
FNavMeshEdgeBase* FNavMeshPolyBase::GetEdgeFromIdx(INT Idx,UNavigationMeshBase* MeshToUse, UBOOL bAllowInvalidEdges)
{
	MeshToUse = MeshToUse ? MeshToUse : NavMesh;
	FNavMeshEdgeBase* Edge = NULL;
	
	// dynamic edges come after normal edges 
	if(Idx<PolyEdges.Num())
	{
		INT EdgeIdx = PolyEdges(Idx);

		if( EdgeIdx == MAXWORD || EdgeIdx == MAXWORD-1)
		{
			return NULL;
		}
		Edge = MeshToUse->GetEdgeAtIdx(EdgeIdx);
	}
	else
	{
		Idx -= PolyEdges.Num();
		static TArray<FNavMeshCrossPylonEdge*> Edges;
		Edges.Reset();
		// dynamic edges are always grabbed from the owning mesh (not the passed mesh)
		NavMesh->DynamicEdges.MultiFind(Item,Edges);
		Edge = Edges(Idx);
	}

	return (!bAllowInvalidEdges && !Edge->IsValid()) ? NULL : Edge;
}

/**
 * UpdateDynamicObstaclesForEdge
 * - this function will ensure that any updates that need to be made are made for the edge at the given Idx
 * @param SessionID - the path session ID for the currently running path (used to determine if we've already checked this edge's poly or not)
 * @param OtherPoly - other poly of the edge (local to this poly
 */
void FNavMeshPolyBase::UpdateDynamicObstaclesForEdge(INT SessionID, FNavMeshPolyBase* OtherPoly)
{
	if(OtherPoly == NULL)
	{
		return;
	}

	// this poly has already been visited by this path search, it's already done
	UNavigationMeshBase* TopMostMesh = OtherPoly->NavMesh->GetTopLevelMesh();
	TopMostMesh->SavedSessionID=SessionID;
}


/**
 * resets this poly's verts, removes all claims to them
 */
void FNavMeshPolyBase::ClearVerts()
{
	TArray<FNavMeshPolyBase*> AdjacentPolys;
	GetAdjacentPolys(AdjacentPolys);

	for(INT VertIdx=0;VertIdx < PolyVerts.Num(); VertIdx++)
	{
		NavMesh->Verts(PolyVerts(VertIdx)).ContainingPolys.RemoveItem(this);
	}
	PolyVerts.Empty();
	BoxBounds.Init();

	if(OctreeId.IsValidId())
	{
		NavMesh->RemovePolyFromOctree(this);
	}

	SetBorderPoly(FALSE,&AdjacentPolys);
}

/** @returns the height of this poly from the poly bounds */
FLOAT FNavMeshPolyBase::GetPolyHeight()
{
	return PolyHeight;
}

FVector APylon::Up(FNavMeshPolyBase* Poly)
{
	if(bImportedMesh)
	{
		return Poly->GetPolyNormal(WORLD_SPACE);
	}
	else
	{
		return FVector(0.f,0.f,1.f);
	}
}

/**
 * Adds the passed verts to a polygon (to be used on empty polygons)
 * @param inVertIndices - vertex IDs to be added to this poly
 */
void FNavMeshPolyBase::AddVerts(const TArray<VERTID>& inVertIndices)
{
	checkSlowish(PolyVerts.Num() == 0);
	PolyVerts = inVertIndices;

	FVector Up = NavMesh->GetPylon()->Up(this);
	PolyCenter = FVector(0.f);
	for(INT NewVertIdx=0;NewVertIdx<inVertIndices.Num();NewVertIdx++)
	{
		FMeshVertex& Vert = NavMesh->Verts(inVertIndices(NewVertIdx));
		Vert.ContainingPolys.AddUniqueItem(this);
		BoxBounds += Vert + Up * PolyHeight;
		BoxBounds += Vert - Up * ExpansionPolyBoundsDownOffset;

		PolyCenter += Vert;
	}
	PolyCenter /= PolyVerts.Num();

	FVector A = NavMesh->GetVertLocation(PolyVerts(0),LOCAL_SPACE);
	FVector B = NavMesh->GetVertLocation(PolyVerts(1),LOCAL_SPACE);
	FVector C = NavMesh->GetVertLocation(PolyVerts(2),LOCAL_SPACE);

	PolyNormal =  CalcNormal();
	checkSlowish(!PolyNormal.IsNearlyZero());
	if(TriangleArea2(A,B,C)<0.f)
	{
		PolyNormal*=-1.0f;
	}
	if(OctreeId.IsValidId())
	{
		NavMesh->RemovePolyFromOctree(this);
	}
	NavMesh->AddPolyToOctree(this);

	if(IsBorderPoly())
	{
		TArray<FNavMeshPolyBase*> AdjacentPolys;
		GetAdjacentPolys(AdjacentPolys);
		SetBorderPoly(TRUE,&AdjacentPolys);
	}
}



void APylon::UpdateComponentsInternal(UBOOL bCollisionUpdate)
{
	// if we have moved, update the transform of the mesh
	if ((GIsEditor && !GIsCooking && !GIsPlayInEditorWorld && !GIsUCC ) || (CylinderComponent != NULL && (CylinderComponent->NeedsReattach() || CylinderComponent->NeedsUpdateTransform())))
	{
		PylonMoved();
	}

	Super::UpdateComponentsInternal(bCollisionUpdate);
}

/**
 * Called from UpdateComponentsInternal when a transform update is needed (when this pylon has moved)
 */
void APylon::PylonMoved()
{
	// update octree position 
	RemoveFromNavigationOctree();
	if(NavMeshPtr != NULL)
	{
		NavMeshPtr->WorldToLocal = GetMeshWorldToLocal();
		NavMeshPtr->LocalToWorld = GetMeshLocalToWorld();
		NavMeshPtr->bNeedsTransform = (!IsStatic() || bImportedMesh);
	}

	if(ObstacleMesh != NULL)
	{
		ObstacleMesh->WorldToLocal = GetMeshWorldToLocal();
		ObstacleMesh->LocalToWorld = GetMeshLocalToWorld();
		ObstacleMesh->bNeedsTransform = (!IsStatic() || bImportedMesh);
	}
	//Pylon bounds are based on NavMesh bounds at runtime
	//thus set the new transformations before adding back to oct tree
	AddToNavigationOctree();
}

/**
* this function returns the local to world matrix transform that should be used for navmeshes associated with this
* pylon 
*/
FMatrix APylon::GetMeshLocalToWorld()
{
	// if we're non-static, or an imported mesh use the real transform
	if (bImportedMesh)
	{
		return LocalToWorld();
	}
	else if(!IsStatic())
	{
		FTranslationMatrix	LToW		( -PrePivot					);
		FRotationMatrix		TempRot		( Rotation					);
		FTranslationMatrix	TempTrans	( Location					);
		LToW *= TempRot;
		LToW *= TempTrans;
		return LToW;
	}

	// otherwise use identity so it's easier to debug (so vertex positions are in WS)
	return FMatrix::Identity;
}

/**
* this function returns the world to local matrix transform that should be used for navmeshes associated with this
* pylon 
*/
FMatrix APylon::GetMeshWorldToLocal()
{
	// if we're non-static, or an imported mesh use the real transform
	if ( bImportedMesh )
	{
		return WorldToLocal();
	}
	else if(!IsStatic())
	{
		return	FTranslationMatrix(-Location) *
			FInverseRotationMatrix(Rotation) *
			FTranslationMatrix(PrePivot);
	}

	// otherwise use identity so it's easier to debug (so vertex positions are in WS)
	return FMatrix::Identity;
}

/////////////////////////////////////////////////////////////////////
// Interface_NavMeshPathSwitch
/////////////////////////////////////////////////////////////////////

INT IInterface_NavMeshPathSwitch::CostFor( const FNavMeshPathParams& PathParams, const FVector& PreviousPoint, FVector& out_PathEdgePoint, FNavMeshPathObjectEdge* Edge, FNavMeshPolyBase* SourcePoly )
{
	AAISwitchablePylon* DestPylon = GetLinkedPylonAtIdx(Edge->InternalPathObjectID);

	return appTrunc((DestPylon->Location - PreviousPoint).Size()*1.5f);
}

// call the switch's support function 
UBOOL IInterface_NavMeshPathSwitch::Supports( const FNavMeshPathParams& PathParams,
										FNavMeshPolyBase* CurPoly,
										FNavMeshPathObjectEdge* Edge,
										FNavMeshEdgeBase* PredecessorEdge)
{

	AAIController* AI = Cast<AAIController>(PathParams.Interface->GetUObjectInterfaceInterface_NavigationHandle());
	// only use this on bots that can use switches, and when we have a gate that needs opening
	return (!IsSwitchOpen() && AI != NULL && CanBotUseThisSwitch(AI));
}

// if bot is in the same poly as the trigger, go to the trigger itself
UBOOL IInterface_NavMeshPathSwitch::GetEdgeDestination(const FNavMeshPathParams& PathParams,
  														FLOAT EntityRadius,
														const FVector& InfluencePosition,
														const FVector& EntityPosition,
														FVector& out_EdgeDest,
														FNavMeshPathObjectEdge* Edge, 
														UNavigationHandle* Handle)
{
	out_EdgeDest = GetDestination(EntityRadius);
	return TRUE;
}

// overidden to activate the switch when the bot needs to 
UBOOL IInterface_NavMeshPathSwitch::PrepareMoveThru( IInterface_NavigationHandle* Interface,
							  FVector& out_MovePt,
							  FNavMeshPathObjectEdge* Edge )
{
	AAIController* AI = Cast<AAIController>(Interface->GetUObjectInterfaceInterface_NavigationHandle());

	AActor* ThisActor = Cast<AActor>(GetUObjectInterfaceInterface_NavMeshPathObject());
	// if the switch is closed, and we're at the proper spot	
	if(AI != NULL && AI->Pawn != NULL && AI->Pawn->ReachedDestination(ThisActor) && !IsSwitchOpen())
	{
		return eventAIActivateSwitch(AI);
	}
	return FALSE;
}

/**
* called to allow this PO to draw custom stuff for edges linked to it
* @param DRSP          - the sceneproxy we're drawing for
* @param DrawOffset    - offset from the actual location we should be drawing 
* @param Edge          - the edge we're drawing
* @return - whether this PO is doing custom drawing for the passed edge (FALSE indicates the default edge drawing functionality should be used)
*/
UBOOL IInterface_NavMeshPathSwitch::DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset, FNavMeshPathObjectEdge* Edge )
{
	APylon* Pylon = NULL;
	for(INT Idx=0;Idx<GetNumLinkedPylons();++Idx)
	{
		Pylon = GetLinkedPylonAtIdx(Idx);
		if(Pylon != NULL)
		{
			new(DRSP->DashedLines) FDebugRenderSceneProxy::FDashedLine(Edge->GetEdgeCenter(),Pylon->Location,FColor(0,255,255),10.0f);
		}
	}

	return TRUE;		
}

/**
* called after edge creation is complete for each pylon to allow this PO to add edges for itself
* @param Py - the pylon which we are creating edges for
*/
void IInterface_NavMeshPathSwitch::CreateEdgesForPathObject( APylon* Py )
{
	FVector Dest = GetDestination(-1.f);
	APylon* MyPylon = NULL;
	FNavMeshPolyBase* MyPoly = NULL;
	if( !UNavigationHandle::GetPylonAndPolyFromPos(Dest,NAVMESHGEN_MIN_WALKABLE_Z,MyPylon,MyPoly))
	{
		return;
	}

	if(MyPylon == Py)
	{
		APylon* Pylon = NULL;
		for(INT Idx=0;Idx<GetNumLinkedPylons();++Idx)
		{
			Pylon = GetLinkedPylonAtIdx(Idx);
			if(Pylon == NULL)
			{
				continue;
			}
			AActor* Act = Cast<AActor>(GetUObjectInterfaceInterface_NavMeshPathObject());
			FNavMeshPolyBase* Poly = Pylon->NavMeshPtr->GetPolyFromPoint(Pylon->Location,NAVMESHGEN_MIN_WALKABLE_Z,WORLD_SPACE);
			if( Poly != NULL )
			{
				AddEdgeForThisPO(Act,MyPylon,MyPoly,Poly,Dest,Dest,Idx);
			}
		}
	}
}
/**
 * Rebuilds all submesh data for any submeshes within the passed top level polygons
 * @param Polys - polys to trigger rebuild for
 */
void IInterface_NavMeshPathObstacle::TriggerRebuildForPassedTLPolys( const TArray<FNavMeshPolyBase*>& Polys )
{
	TArray<APylon*> Pylons;
	FNavMeshPolyBase* CurPoly = NULL;
	for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
	{
		CurPoly = Polys(PolyIdx);

		checkSlowish(CurPoly->NumObstaclesAffectingThisPoly > 0);

		// MT-NOTE: dynamic obstacles on dynamic meshes not currently supported (perhaps only support when obstacle is based on mesh's base?)
		if(!CurPoly->NavMesh->GetPylon()->IsStatic())
		{
			//warnf( TEXT("RegisterObstacleWithPolys dynamic pylon") );
			continue;
		}

		FPolyObstacleInfo* CurInfo = CurPoly->NavMesh->PolyObstacleInfoMap.Find(CurPoly->Item);


		if(CurInfo != NULL)
		{
			CurInfo->MarkNeedsRebuild();
		}

		Pylons.AddUniqueItem(CurInfo->Poly->NavMesh->GetPylon());
	}


	// update any meshes that need updating 
	UpdateAllDynamicObstaclesInPylonList(Pylons);
}
/**
 * this will register the passed list of obstacles, and then perform a build at the end.
 * useful for registering a big list of obstacles all at once and paying much less cost (mesh doesn't have to be rebuilt each registration, only at the end)
 * @param ObstaclesToRegister - list of obstacles to register
 * @return - TRUE if registration and build was successful
 */
#define CAPTURE_DYNAMIC_NAV_MESH_PERF 0
UBOOL IInterface_NavMeshPathObstacle::RegisterObstacleListWithNavMesh(TArray<IInterface_NavMeshPathObstacle*>& Obstacles)
{
	SCOPE_QUICK_TIMERX(RegisterObstacleListWithNavMesh,FALSE)


#if CAPTURE_DYNAMIC_NAV_MESH_PERF
	static UBOOL bDoIt = FALSE;
	if( bDoIt == TRUE )
	{
		GCurrentTraceName = NAME_Game;
	}
	else
	{
		GCurrentTraceName = NAME_None;
	}

	appStartCPUTrace( NAME_Game, FALSE, TRUE, 40, NULL );
#endif

	// hold edge deletes until all processing is done to avoid triggering script calls in the middle of things
	SCOPED_EDGE_DELETE_HOLD(RegisterObstacleListWithNavMesh)

	static TArray<APylon*> Pylons;
	Pylons.Reset();

	UBOOL bFailedReg = FALSE;
	for( INT ObstacleIdx=0;ObstacleIdx<Obstacles.Num();++ObstacleIdx)
	{
		IInterface_NavMeshPathObstacle* CurObstacle = Obstacles(ObstacleIdx);
		
		if ( FNavMeshWorld::GetNavMeshWorld()->ActiveObstacles.Find(CurObstacle)!=NULL )
		{
			warnf(TEXT("Warning (%s) just tried to register itself with the navmesh, but it already was!  You must first unregister before re-registering an object"),*CurObstacle->GetUObjectInterfaceInterface_NavMeshPathObstacle()->GetName());
			continue;
		}

		FBox PolyBounds(0);

		static TArray<FVector> BoundingShape;

		for(INT ShapeIdx=0;ShapeIdx<CurObstacle->GetNumBoundingShapes();++ShapeIdx)
		{
			BoundingShape.Reset();

			if(!CurObstacle->GetBoundingShape(BoundingShape,ShapeIdx))
			{
				continue;
			}

			for(INT Idx=0;Idx<BoundingShape.Num();++Idx)
			{
				PolyBounds += BoundingShape(Idx);
				PolyBounds += BoundingShape(Idx)+FVector(0.f,0.f,10.f);
			}

			FVector Ctr(0.f), Extent(0.f);

			PolyBounds.GetCenterAndExtents(Ctr,Extent);

			static TArray<FNavMeshPolyBase*> Polys;
			Polys.Reset();
			UNavigationHandle::GetAllOverlappingPylonsFromBox(Ctr,Extent,Pylons);

			APylon* CurPylon = NULL;
			for(INT PylonIdx=0;PylonIdx<Pylons.Num();++PylonIdx)
			{
				CurPylon = Pylons(PylonIdx);

				if( CurPylon->IsValid() )
				{
					CurPylon->GetPolysAffectedByObstacleShape(CurObstacle,BoundingShape,Ctr,Extent,Polys);
				}
			}

			if( !CurObstacle->RegisterObstacleWithPolys(BoundingShape,Polys) )
			{
				bFailedReg=TRUE;
			}
		}

	}

	
	// update any meshes that need updating 
	UpdateAllDynamicObstaclesInPylonList(Pylons);

#if CAPTURE_DYNAMIC_NAV_MESH_PERF
	appStopCPUTrace( NAME_Game );
#endif

	return !bFailedReg;
}


/**
 * will get a list of polys which should be affected by the passed obstacle boundary shape 
 * @param Shape - the convex poly shape to test for
 * @param out_Polys - the list of polys which we should add to 
 */
void APylon::GetPolysAffectedByObstacleShape(IInterface_NavMeshPathObstacle* Obstacle, const TArray<FVector>& Shape, const FVector& ShapeBoundsCtr,const FVector& ShapeBoundsExtent, TArray<FNavMeshPolyBase*>& out_Polys)
{
	GetIntersectingPolys(ShapeBoundsCtr,ShapeBoundsExtent,out_Polys,TRUE,TRUE);
}

/**
* this will register this shape with the obstacle mesh, indicating it should be considered
* when generating paths
* @return - TRUE If registration was successful
*/
UBOOL IInterface_NavMeshPathObstacle::RegisterObstacleWithNavMesh()
{
	// So you can toggle this code with one simple change
#if CAPTURE_DYNAMIC_NAV_MESH_PERF
	static UBOOL bDoIt = FALSE;
	if( bDoIt == TRUE )
	{
		GCurrentTraceName = NAME_Game;
	}
	else
	{
		GCurrentTraceName = NAME_None;
	}

	appStartCPUTrace( NAME_Game, FALSE, TRUE, 40, NULL );
#endif

	// hold edge deletes until all processing is done to avoid triggering script calls in the middle of things
	SCOPED_EDGE_DELETE_HOLD(RegisterObstacleWithNavMesh)

	SCOPE_QUICK_TIMERX(RegisterObstacleWithNavMesh,FALSE)


	if ( FNavMeshWorld::GetNavMeshWorld()->ActiveObstacles.Find(this)!=NULL )
	{
		warnf(TEXT("Warning (%s) just tried to register itself with the navmesh, but it already was!  You must first unregister before re-registering an object"),*GetUObjectInterfaceInterface_NavMeshPathObstacle()->GetName());
		return FALSE;
	}

	FBox PolyBounds(0);

	static TArray<FVector> BoundingShape;
	static TArray<APylon*> Pylons;
	Pylons.Reset();

	UBOOL bRegisterStatus = TRUE;
	for(INT ShapeIdx=0;ShapeIdx<GetNumBoundingShapes();++ShapeIdx)
	{
		BoundingShape.Reset();

		if(!GetBoundingShape(BoundingShape,ShapeIdx))
		{
			return FALSE;
		}

		for(INT Idx=0;Idx<BoundingShape.Num();++Idx)
		{
			PolyBounds += BoundingShape(Idx);
			PolyBounds += BoundingShape(Idx)+FVector(0.f,0.f,10.f);
		}


		static TArray<FNavMeshPolyBase*> Polys;
		Polys.Reset();

		FVector Ctr(0.f), Extent(0.f);
		PolyBounds.GetCenterAndExtents(Ctr,Extent);

		UNavigationHandle::GetAllOverlappingPylonsFromBox(Ctr,Extent,Pylons);

		APylon* CurPylon = NULL;
		for(INT PylonIdx=0;PylonIdx<Pylons.Num();++PylonIdx)
		{
			CurPylon = Pylons(PylonIdx);

			if( CurPylon->IsValid() )
			{
				CurPylon->GetPolysAffectedByObstacleShape(this,BoundingShape,Ctr,Extent,Polys);
			}
		}
		
		if ( !RegisterObstacleWithPolys(BoundingShape,Polys) )
		{
			bRegisterStatus=FALSE;
		}
	}
	
	// update any meshes that need updating 
	UpdateAllDynamicObstaclesInPylonList(Pylons);

#if CAPTURE_DYNAMIC_NAV_MESH_PERF
	appStopCPUTrace( NAME_Game );
#endif

	return bRegisterStatus;
}


/**
 * this is called on polys which have just had all obstacles cleared and won't get a normal build step
 * and thus need to have edges created to adjacent sub-meshes
 */
UBOOL IInterface_NavMeshPathObstacle::DoEdgeFixupForNewlyClearedPolys(const TArray<FNavMeshPolyBase*> PolysThatNeedFixup)
{
	if( !GIsRunning )
	{
		return FALSE;
	}

	FNavMeshPolyBase* CurPoly = NULL;
	for(INT Idx=0;Idx<PolysThatNeedFixup.Num();++Idx)
	{
		CurPoly = PolysThatNeedFixup(Idx);
		CurPoly->NavMesh->BuildSubMeshEdgesForJustClearedTLPoly(CurPoly->Item);
	}
	return TRUE;
}

/**
 * given a list of pylons will update all the obstacles that need updating within it
 * also does post steps after update is finished
 * @param Pylons - list of pylons to update obstacles for
 */
void IInterface_NavMeshPathObstacle::UpdateAllDynamicObstaclesInPylonList(TArray<APylon*>& Pylons)
{
	if( !GIsRunning && GWorld->GetTimeSeconds() > 0.f )
	{
		return;
	}
	// make sure edge delete notifications aren't sent out while we're building
	SCOPED_EDGE_DELETE_HOLD(UpdateAllDynamicObstaclesInPylonList)


	//SCOPE_QUICK_TIMERX(UpdateAllDynamicObstaclesInPylonList,FALSE)

	APylon* CurPylon = NULL;
	// stage one, build all geo
	static TArray<FPolyObstacleInfo*> ObstaclesThatWereJustBuilt;
	ObstaclesThatWereJustBuilt.Reset();

	for(INT PylonIdx=0;PylonIdx<Pylons.Num();++PylonIdx)
	{
		CurPylon = Pylons(PylonIdx);

		if( CurPylon->IsValid() )
		{
			if( CurPylon->NavMeshPtr != NULL)
			{
				CurPylon->NavMeshPtr->BuildAllSubMeshGeometry(ObstaclesThatWereJustBuilt);
			}
		}
	}

	// stage two, finish build on all submeshes
	for(INT PylonIdx=0;PylonIdx<Pylons.Num();++PylonIdx)
	{
		CurPylon = Pylons(PylonIdx);

		if( CurPylon->IsValid() )
		{
			if( CurPylon->NavMeshPtr != NULL)
			{
				CurPylon->NavMeshPtr->FinishSubMeshBuilds(ObstaclesThatWereJustBuilt);
			}
		}
	}

	// notify all obstacles whose adjacent meshes were just rebuilt that they were rebuilt
	// (e.g. give them a chance to do any extra work.. add special edges, etc)

	for(INT Idx=0;Idx<ObstaclesThatWereJustBuilt.Num();++Idx)
	{
		FPolyObstacleInfo* Info = ObstaclesThatWereJustBuilt(Idx);

		if(Info != NULL && Info->SubMesh != NULL)
		{
			// iterate through all edges on the poly and see if they need special handling post submesh creation
			for(INT OldPolyEdgeIdx=0;OldPolyEdgeIdx<Info->Poly->GetNumEdges();++OldPolyEdgeIdx)
			{
				FNavMeshEdgeBase* CurEdge = Info->Poly->GetEdgeFromIdx(OldPolyEdgeIdx,Info->Poly->NavMesh,TRUE);
				if(CurEdge->IsValid(TRUE))
				{
					CurEdge->PostSubMeshUpdateForOwningPoly(Info->Poly,Info->SubMesh);
				}
			}

			Info->Poly->NavMesh->RebuildMetaDataLinkageForSubMesh(Info->Poly->Item,Info->SubMesh);


			for(INT ObstacleIdx=0;ObstacleIdx<Info->LinkedObstacles.Num();++ObstacleIdx)
			{
				IInterface_NavMeshPathObstacle* Obst = Info->LinkedObstacles(ObstacleIdx);
				if(Obst != NULL)
				{
					Obst->PostSubMeshUpdateForTopLevelMesh(Info->SubMesh);
				}
			}
		}			
	}
}

/** 
 * called when the owner of this interface is being unloaded or destroyed and this obstacle needs to be cleaned up
 */
void IInterface_NavMeshPathObstacle::CleanupOnRemoval()
{
	UnregisterObstacleWithNavMesh();
}

UBOOL IInterface_NavMeshPathObstacle::RegisterObstacleWithPolys( const TArray<FVector>& BoundingShape, const TArray<FNavMeshPolyBase*>& Polys)
{
	FNavMeshPolyBase* CurPoly = NULL;
	for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
	{
		CurPoly = Polys(PolyIdx);

		// MT-NOTE: dynamic obstacles on dynamic meshes not currently supported (perhaps only support when obstacle is based on mesh's base?)
		if(!CurPoly->NavMesh->GetPylon()->CompatibleWithDynamicObstacles())
		{
			//warnf( TEXT("RegisterObstacleWithPolys dynamic pylon") );
			continue;
		}

		if(CurPoly->IntersectsPoly(BoundingShape,WORLD_SPACE))
		{
			FPolyObstacleInfo* CurInfo = CurPoly->NavMesh->PolyObstacleInfoMap.Find(CurPoly->Item);


			if(CurInfo != NULL)
			{
				CurInfo->AddLinkedObstacle(this);
			}
			else
			{
				FPolyObstacleInfo Info(CurPoly);
				Info.AddLinkedObstacle(this);
				CurPoly->NavMesh->PolyObstacleInfoMap.Set(CurPoly->Item,Info);
			}

			FNavMeshWorld::GetNavMeshWorld()->ActiveObstacles.AddUnique(this,FPolyReference(CurPoly->NavMesh->GetPylon(),CurPoly->Item));
		}
	}

	// if this was not attached to any pylons add a blank entry so we know it's active
	// because the level with the pylon may not be the same as the one with the obstacle
	// we save them off in that case so when a pylon is loaded it can get updates with existing obstacles
	if(Polys.Num() < 1)
	{
		FNavMeshWorld::GetNavMeshWorld()->ActiveObstacles.AddUnique(this,FPolyReference());
		//warnf( TEXT("We were not attached to any pylon.  Adding a blank entry.") );
	}

	return TRUE;
}


/**
* this will remove this shape from the obstacle mesh, indicating it is no longer relevant to 
* generating paths
* @return TRUE if unregistration was successful
*/
UBOOL IInterface_NavMeshPathObstacle::UnregisterObstacleWithNavMesh()
{
	// hold edge deletes until all processing is done to avoid triggering script calls in the middle of things
	SCOPED_EDGE_DELETE_HOLD(UnregisterObstacleWithNavMesh)

	FNavMeshWorld* World = FNavMeshWorld::GetNavMeshWorld();

	if(World == NULL)
	{
		return FALSE;
	}

	// grab the list of all polys associated with this obstacle so we can remove it from those poly lists
	static TArray<FPolyReference> Polys;
	Polys.Reset();
	World->ActiveObstacles.MultiFind(this,Polys);
	FNavMeshPolyBase* CurPoly = NULL;

	static TArray<FNavMeshPolyBase*> JustClearedTopLevelPolys;
	JustClearedTopLevelPolys.Reset();

	// make sure edge delete notifications aren't sent out while we're cleaning up!
	{
		SCOPED_EDGE_DELETE_HOLD(UnregisterObstacleWithNavMesh)

		for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
		{
			CurPoly = Polys(PolyIdx).GetPoly(TRUE);
			if(CurPoly != NULL)
			{
				FPolyObstacleInfo* CurInfo = CurPoly->NavMesh->PolyObstacleInfoMap.Find(CurPoly->Item);
				if(CurInfo != NULL)
				{
					CurInfo->RemoveLinkedObstacle(this);
				}

				// if we just removed the last one, add it to 'just cleared' list to be post-processed
				if(CurPoly->NumObstaclesAffectingThisPoly == 0)
				{
					JustClearedTopLevelPolys.AddItem(CurPoly);
				}
			}
		}

		World->ActiveObstacles.RemoveKey(this);
	}

	// update status of navmesh
	CurPoly = NULL;
	static TArray<APylon*> Pylons;
	Pylons.Reset();
	for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
	{
		CurPoly = *Polys(PolyIdx);
		if(CurPoly != NULL)
		{
			Pylons.AddUniqueItem(CurPoly->NavMesh->GetPylon());
		}
	}

	UpdateAllDynamicObstaclesInPylonList(Pylons);

	DoEdgeFixupForNewlyClearedPolys(JustClearedTopLevelPolys);

	for(INT Idx=0;Idx< JustClearedTopLevelPolys.Num(); ++ Idx)
	{
		// since these are now empty, remove the info from the map
		FNavMeshPolyBase* CurPoly = JustClearedTopLevelPolys(Idx);
		CurPoly->NavMesh->PolyObstacleInfoMap.Remove(CurPoly->Item);
	}

	return TRUE;
}

/**
 * This function is called when an edge is going to be added connecting a polygon internal to this obstacle to another polygon which is not
 * Default behavior just a normal edge, override to add special costs or behavior (e.g. link a pathobject to the obstacle)
 * @param Status - current status of edges (e.g. what still needs adding)	 
 * @param inV1 - vertex location of first vert in the edge
 * @param inV2 - vertex location of second vert in the edge
 * @param ConnectedPolys - the polys this edge links
 * @param bEdgesNeedToBeDynamic - whether or not added edges need to be dynamic (e.g. we're adding edges between meshes)
 * @param PolyAssocatedWithThisPO - the index into the connected polys array parmaeter which tells us which poly from that array is associated with this path object
 * @(optional) param SupportedEdgeWidth - width of unit that this edge supports, defaults to -1.0f meaning the length of the edge itself will be used
 * @(optional) param EdgeGroupID - if htis edge is in an edge group, the ID of the edge group
 * @return returns an enum describing what just happened (what actions did we take) - used to determien what accompanying actions need to be taken 
 *         by other obstacles and calling code
 */
EEdgeHandlingStatus IInterface_NavMeshPathObstacle::AddObstacleEdge( EEdgeHandlingStatus Status, const FVector& inV1, const FVector& inV2, TArray<FNavMeshPolyBase*>& ConnectedPolys, UBOOL bEdgesNeedToBeDynamic, INT PolyAssocatedWithThisPO, FLOAT SupportedEdgeWidth/*=1.0f*/, BYTE EdgeGroupID)
{
	return EHS_AddedNone;
}

/** 
 *  This is called offline when edges are about to be added from the exterior of the pathobject to the interior or vice versa
 * Default behavior just a normal edge, override to add special costs or behavior 
 * @param Status - current status of edges (e.g. what still needs adding)	 
 * @param inV1 - vertex location of first vert in the edge
 * @param inV2 - vertex location of second vert in the edge
 * @param ConnectedPolys - the polys this edge links
 * @param PolyAssocatedWithThisPO - the index into the connected polys array parmaeter which tells us which poly from that array is associated with this path object
 * @(optional) param SupportedEdgeWidth - width of unit that this edge supports, defaults to -1.0f meaning the length of the edge itself will be used
 * @(optional) param EdgeGroupID - if htis edge is in an edge group, the ID of the edge group
 * @return returns an enum describing what just happened (what actions did we take) - used to determien what accompanying actions need to be taken 
 *         by other obstacles and calling code
 */
EEdgeHandlingStatus IInterface_NavMeshPathObject::AddStaticEdgeIntoThisPO( EEdgeHandlingStatus Status, const FVector& inV1, const FVector& inV2, TArray<FNavMeshPolyBase*>& ConnectedPolys, INT PolyAssocatedWithThisPO, FLOAT SupportedEdgeWidth/*=1.0f*/, BYTE EdgeGroupID)
{
	return EHS_AddedNone;
}



// assumed incoming edge is in WS
UBOOL bOnlySplitEdgeIntersectors=TRUE;
UBOOL bCheckPlaneAABB=TRUE;
UBOOL UNavigationMeshBase::SplitMeshAboutEdge(const FVector& EdgePt0, const FVector& EdgePt1, const FVector& Up, FLOAT Height)
{
	//GWorld->GetWorldInfo()->DrawDebugLine(EdgePt0,EdgePt1,255,255,0,TRUE);
	// for each polygon in the mesh,
	//   if it its bounds intersect the plane, split the poly about the plane
	FNavMeshPolyBase* CurPoly = NULL;
	UBOOL bSplitAtLestOne = FALSE;

	FVector EdgeCtr = (EdgePt0+EdgePt1)*0.5f;
	FVector EdgeDir = (EdgePt0-EdgePt1);
	FPlane SplitPlane( EdgeCtr, (EdgeDir^Up).SafeNormal());

	FVector LS_EdgePt0 = W2LTransformFVector(EdgePt0);
	FVector LS_EdgePt1 = W2LTransformFVector(EdgePt1);

	FVector EntryPt(0.f),ExitPt(0.f);
	for(PolyList::TIterator It(BuildPolys.GetTail());It;)
	{
		CurPoly = *It;
		--It;
		FBox TransformedBox = CurPoly->GetPolyBounds();

		UBOOL bHeightEarlyOut=FALSE;
		if(Height > 0.f)
		{
			FBox EdgeBox(0);
			EdgeBox += EdgePt0;
			EdgeBox += EdgePt0 + FVector(0.f,0.f,Height);
			EdgeBox += EdgePt1;
			EdgeBox += EdgePt1 + FVector(0.f,0.f,Height);

			bHeightEarlyOut = !TransformedBox.Intersect(EdgeBox);
		}
		 
		if(!bHeightEarlyOut && (!bCheckPlaneAABB||FPlaneAABBIsect(SplitPlane,TransformedBox)) && (!bOnlySplitEdgeIntersectors||CurPoly->IntersectsPoly2D(LS_EdgePt0,LS_EdgePt1,EntryPt,ExitPt,LOCAL_SPACE)))
		{
			static TArray<FVector> Poly1;
			Poly1.Reset();
			static TArray<FVector> Poly2;
			Poly2.Reset();


			if(SplitPolyAlongPlane(CurPoly,SplitPlane,Poly1,Poly2))
			{				
				bSplitAtLestOne = TRUE;

				// if new poly is big enough, add to new mesh
				if(FNavMeshPolyBase::CalcArea(Poly1) > NAVMESHGEN_MIN_POLY_AREA)
				{
					AddPoly(Poly1,CurPoly->PolyHeight,WORLD_SPACE);
				}

				// if new poly is big enough, add to new mesh
				if(FNavMeshPolyBase::CalcArea(Poly2) > NAVMESHGEN_MIN_POLY_AREA)
				{
					AddPoly(Poly2,CurPoly->PolyHeight,WORLD_SPACE);
				}
				RemovePoly(CurPoly);

			}
		}
	}
	 		
	return bSplitAtLestOne;
}


IMPLEMENT_COMPARE_CONSTREF(PS3CompilerFix, SmallSplitsFirst, { return (FNavMeshPolyBase::CalcArea(A.Shape) > FNavMeshPolyBase::CalcArea(B.Shape)); })

UBOOL UNavigationMeshBase::SplitMeshAboutPathObjects()
{
	// ** First, build a list of path objects which affect this mesh
	TArray<FMeshSplitingShape> AffectingPOs;

	for(INT PathObjectIdx=0;PathObjectIdx<PathObjects.Num();++PathObjectIdx)
	{
		IInterface_NavMeshPathObject* CurrentPO = PathObjects(PathObjectIdx);
		FMeshSplitingShape SplitShape;
		if(CurrentPO->GetMeshSplittingPoly(SplitShape.Shape,SplitShape.Height))
		{
			FNavMeshPolyBase* IntersectingPoly=NULL;
			if(IntersectsPoly(SplitShape.Shape,IntersectingPoly,NULL,TRUE))
			{
				SplitShape.bKeepInternalGeo=TRUE;
				//SplitShape.ID = AffectingPOs.Num();
				AffectingPOs.AddItem(SplitShape);
			}
		}
	}	

	// ** sort list based on area so that the small ones get split first 
	//   (otherwise big shapes containing other shapes would exclude the little guys)
	Sort<USE_COMPARE_CONSTREF(PS3CompilerFix,SmallSplitsFirst)>(&AffectingPOs(0),AffectingPOs.Num());
	// set IDs
	for(INT Idx=0;Idx<AffectingPOs.Num();++Idx)
	{
		AffectingPOs(Idx).ID = Idx;
	}
	
	// ** split split split!
	TArray<FMeshPolySplitShapePair> InternalMeshes;
	if(!SplitMeshAboutShapes(AffectingPOs,InternalMeshes))
	{
		return FALSE;
	}

	// ** copy internal meshes back into larger mesh
	for(INT InternalMeshIdx=0;InternalMeshIdx<InternalMeshes.Num();++InternalMeshIdx)
	{
		FMeshPolySplitShapePair& Pair = InternalMeshes(InternalMeshIdx);
		for(PolyList::TIterator It(Pair.Mesh->BuildPolys.GetTail());It;)
		{	
			FNavMeshPolyBase* CurPoly = *It;
			--It;
			CopyPolyIntoMesh(CurPoly);			
		}
	}
	return TRUE;
}

void RemoveDynamicEdgesThatRefThisMesh(UNavigationMeshBase* MeshToRemoveFrom, UNavigationMeshBase* MeshToRemoveRefsTo)
{
	for(DynamicEdgeList::TIterator Itt(MeshToRemoveFrom->DynamicEdges);Itt;++Itt)
	{
		FNavMeshCrossPylonEdge* Edge = Itt.Value();		
		FNavMeshPolyBase* Poly0 = Edge->GetPoly0();
		FNavMeshPolyBase* Poly1 = Edge->GetPoly1();

		if( (Poly0 != NULL && Poly0->NavMesh == MeshToRemoveRefsTo) || (Poly1 != NULL && Poly1->NavMesh == MeshToRemoveRefsTo) )
		{
			MeshToRemoveFrom->RemoveDynamicCrossPylonEdge(Edge);
		}
	}
}

/**
* this function will notify any AIs who are using edges from this mesh that it's about to be deleted
*/
void UNavigationMeshBase::CleanupMeshReferences(FPolyObstacleInfo* ObstacleInfoForThisMesh/*=NULL*/)
{
	// cleanup standing poly references to anyone inside this mesh
	FNavMeshWorld* World = FNavMeshWorld::GetNavMeshWorld();
	if(World != NULL)
	{
		UNavigationHandle* Handle = NULL;

		for(INT Idx=0;Idx < World->ActiveHandles.Num();++Idx)
		{
			Handle = World->ActiveHandles(Idx);
			if(Handle != NULL)
			{
				if(Handle->SubGoal_DestPoly != NULL && Handle->SubGoal_DestPoly->NavMesh == this)
				{
					Handle->SubGoal_DestPoly = NULL;
				}
				if(Handle->AnchorPoly != NULL && Handle->AnchorPoly->NavMesh == this)
				{
					Handle->AnchorPoly = NULL;
				}
				if(Handle->CurrentEdge != NULL && Handle->CurrentEdge->NavMesh == this)
				{
					Handle->CurrentEdge = NULL;
				}
			}
		}
	}

	if(IsSubMesh())
	{
		for(INT EdgeIdx=0;EdgeIdx<GetNumEdges();++EdgeIdx)
		{
			// if mesh got cleaned up out from under us break out
			if(bMeshHasBeenCleanedUp)
			{
				return;
			}

			FNavMeshEdgeBase* Edge = GetEdgeAtIdx(EdgeIdx);
			if(Edge != NULL)
			{
				FNavMeshWorld::DestroyEdge(Edge,TRUE);
			}
		}
	}
	
	// iterate over all edges that are referencing this mesh and delete them (make local copy of edges because removing the edge is going to remove it from our list while we're iterating)
	static TArray<FNavMeshCrossPylonEdge*> Edges_that_ref_me;
	Edges_that_ref_me.Reset();
	for(IncomingEdgeListType::TIterator It(IncomingDynamicEdges.GetHead());It;++It)
	{
		Edges_that_ref_me.AddItem(*It);
	}

	for(INT EdgeIdx=0;EdgeIdx<Edges_that_ref_me.Num();++EdgeIdx)
	{
		FNavMeshCrossPylonEdge* CurEdge = Edges_that_ref_me(EdgeIdx);

		CurEdge->NavMesh->RemoveDynamicCrossPylonEdge(CurEdge);
	}

	APylon* MyPylon = GetPylon();
	if(IsSubMesh() && MyPylon != NULL && MyPylon->NavMeshPtr != NULL && MyPylon->ObstacleMesh != NULL && MyPylon->DynamicObstacleMesh!= NULL)
	{
		RemovePolysFromDynamicObstacleMeshForMe(ObstacleInfoForThisMesh);	
	}

	FlushDynamicEdges();

	if( GIsGame && !GIsPlayInEditorWorld )
	{
		check(IncomingDynamicEdges.Num() == 0);
	}
	else
	{
		// in the editor poly destination references will be cleared first by the GC so the ref markers won't get cleared out by RemoveDynamicCrossPylonEdge
		// even though the edge is correctly deleted
		IncomingDynamicEdges.Clear();
	}


	// debug -- iterate through all edges and check that none of them are referencing me
#if _DEBUG
	if(GIsGame && GWorld && GWorld->GetWorldInfo())
	{

		TArray<FNavMeshCrossPylonEdge*> Edges;
		for( APylon* CurPylon = GWorld->GetWorldInfo()->PylonList; CurPylon != NULL; CurPylon = CurPylon->NextPylon )
		{
			UNavigationMeshBase* Mesh = CurPylon->NavMeshPtr;
			TArray<FNavMeshCrossPylonEdge*> Cur_Edges;

			Mesh->DynamicEdges.GenerateValueArray(Cur_Edges);
			Edges.Append(Cur_Edges);


			// grab all dynamic edges from submeshes
			for( PolyObstacleInfoList::TIterator It(Mesh->PolyObstacleInfoMap);It;++It)
			{
				FPolyObstacleInfo& Info = It.Value();

				if(Info.SubMesh != NULL)
				{
					Cur_Edges.Reset();
					Info.SubMesh->DynamicEdges.GenerateValueArray(Cur_Edges);
					Edges.Append(Cur_Edges);
				}

			}
		}

		for(INT Idx=0;Idx<Edges.Num();++Idx)
		{
			FNavMeshCrossPylonEdge* Edge = Edges(Idx);
			FNavMeshPolyBase* Poly0 = Edge->GetPoly0();
			FNavMeshPolyBase* Poly1 = Edge->GetPoly1();

			check(!(Poly0 != NULL && Poly0->NavMesh == this) || (Poly1 != NULL && Poly1->NavMesh == this) );
		}
	}
#endif


	bMeshHasBeenCleanedUp=TRUE;
}

/**
 * removes polys in a dynamic obstacle mesh that were added for this navmesh
 * @param ObstacleInfoForThisMesh - the obstacleinfo associated with this mesh (because it's a submesh)
 */
void UNavigationMeshBase::RemovePolysFromDynamicObstacleMeshForMe(FPolyObstacleInfo* ObstacleInfoForThisMesh)
{
	// if we have no info, look it up
	if(ObstacleInfoForThisMesh == NULL)
	{
		UNavigationMeshBase* ParentMesh = GetTopLevelMesh();
		if( ParentMesh != NULL )
		{
			WORD* ParentID = ParentMesh->SubMeshToParentPolyMap.Find(this);
			if( ParentID != NULL )
			{
				ObstacleInfoForThisMesh = PolyObstacleInfoMap.Find(*ParentID);
			}
		}
	}

	// remove any polys in the dynamic obstacle mesh that were added for this submesh
	if( ObstacleInfoForThisMesh != NULL )
	{
		UNavigationMeshBase* OwningMesh = NULL;
		FNavMeshPolyBase* CurrentPoly = NULL;
		for(PolyList::TIterator It(ObstacleInfoForThisMesh->ObstacleMeshPolys.GetHead()); It; ++It)
		{
			CurrentPoly = *It;
			if( CurrentPoly != NULL )
			{
				OwningMesh = CurrentPoly->NavMesh;

				// TODO STEVEP SAMS - This is a placeholder to avoid crashes due to attempting to remove polys from the kDOP that were actually
				//   never there in the first place due to the fix for the case of the infinite loop due to the tri count going above MAXWORD
				//   Consider removing this once that fix is in place
				if (OwningMesh->BuildPolyIndexMap.Num() > CurrentPoly->Item)
				{
					OwningMesh->BuildPolyIndexMap(CurrentPoly->Item) = NULL;
					// NOTE: removing the poly from the buildpolyindexmap will ensure the geo is not considered 
					//       in the KDOP, so there is no need to rebuild it for removals, only adds
					OwningMesh->RemovePoly(CurrentPoly);				
				}
			}
		}
		ObstacleInfoForThisMesh->ObstacleMeshPolys.Clear();
	}
}



/** 
* Setter for setting whether this mesh needs obstacle recompute o rnot
* @param bNeedsRecompute - incoming bool indicating if this mesh needs recompute
*/
void UNavigationMeshBase::SetNeedsRecompute(UBOOL bNeedsRecompute)
{
	bNeedsObstacleRecompute = bNeedsRecompute;
	if(GetPylon()->RenderingComp != NULL && !GetPylon()->RenderingComp->HasAnyFlags(RF_Unreachable))
	{
		GetPylon()->RenderingComp->BeginDeferredReattach();
	}
}

/**
 * MarkNeedsRebuild
 * marks this info as needing to be rebuilt
 */
void FPolyObstacleInfo::MarkNeedsRebuild()
{
	bNeedRecompute = TRUE;
	Poly->NavMesh->SetNeedsRecompute(TRUE);
}

/**
* this will add the passed obstacle to the list of obstacles that are affecting the poly
* attached to this ObstacleInfo
* Note: will mark both the poly and the mesh as needing obstacle rebuilds
* @param Obst - the interface of the obstacle which is affecting this poly
*/
void FPolyObstacleInfo::AddLinkedObstacle(IInterface_NavMeshPathObstacle* Obst)
{
	if(LinkedObstacles.ContainsItem(Obst))
	{
		return;
	}
	LinkedObstacles.AddItem(Obst);
	MarkNeedsRebuild();
	Poly->NumObstaclesAffectingThisPoly++;
}

/**
* this removes association of the passed obstacle with the poly attached to this obstacle info
* Note: will mark both the poly and the mesh as needing obstacle rebuilds
* @param Obst - the obstacle to remove
*/
void FPolyObstacleInfo::RemoveLinkedObstacle(IInterface_NavMeshPathObstacle* Obst)
{
	INT Idx = INDEX_NONE;
	if(!LinkedObstacles.FindItem(Obst,Idx))
	{
		return;
	}
	LinkedObstacles.RemoveSwap(Idx);
	MarkNeedsRebuild();
	if(SubMesh != NULL)
	{
		SubMesh->CleanupMeshReferences(this);
		SubMesh=NULL;
	}
	Poly->NumObstaclesAffectingThisPoly--;
}



UBOOL UNavigationMeshBase::SplitMeshAboutShapes(const TArray<FMeshSplitingShape>& Shapes, TArray<FMeshPolySplitShapePair>& out_InternalMeshes)
{
	
	// ** for each shape we're splitting, split the mesh along each edge of the passed shape
	//    remove geo internal to shapes which don't need it to be preserved
	for(INT ShapeIdx=0;ShapeIdx<Shapes.Num();++ShapeIdx)
	{
		const FMeshSplitingShape& CurShapeInfo = Shapes(ShapeIdx);
		
		FVector PolyNorm = FNavMeshPolyBase::CalcNormal(CurShapeInfo.Shape);

		// split mesh against each edge
		for(INT BorderVertIdx=0;BorderVertIdx<CurShapeInfo.Shape.Num();++BorderVertIdx)
		{
			FVector ThisVert = CurShapeInfo.Shape(BorderVertIdx);
			FVector NextVert = CurShapeInfo.Shape( (BorderVertIdx+1) % CurShapeInfo.Shape.Num());

			SplitMeshAboutEdge(ThisVert,NextVert,PolyNorm);
		}

		// remove polys inside the shape if it doesn't care about them (so we don't have to split them later) 
		if(!CurShapeInfo.bKeepInternalGeo)
		{
			for(PolyList::TIterator It(BuildPolys.GetTail());It;)
			{	
				FNavMeshPolyBase* CurPoly = *It;
				--It;
				// there is some slight snapping that goes on when creating sub-meshes so it's possible adjacent polys are barely colliding
				// with border shape that should not be.. test against poly center to avoid false positives
				if(CurPoly->IntersectsPoly(CurShapeInfo.Shape,WORLD_SPACE) && FNavMeshPolyBase::ContainsPoint(CurShapeInfo.Shape,CurPoly->GetPolyCenter(WORLD_SPACE)))
				{				
					RemovePoly(CurPoly);
				}
			}
		}
	}

	// for each interal geo preserving shape
	// find all polys that intersect with the shape and put them in a temporary mesh, and remove them from the real mesh
	// add temporary mesh to an array to be added back to the mesh after simplification

	// for each shape we just split by
	for(INT AffectingShapeIdx=0;AffectingShapeIdx<Shapes.Num();++AffectingShapeIdx)
	{
		const FMeshSplitingShape& CurShapeInfo = Shapes(AffectingShapeIdx);

		// this is a shape we didn't delete internal geo for
		if(CurShapeInfo.bKeepInternalGeo)
		{
			static TArray<FNavMeshPolyBase*> Polys;
			Polys.Reset();

			GetIntersectingPolys(CurShapeInfo.Shape,Polys,WORLD_SPACE);
			if(Polys.Num() < 1)
			{
				continue;
			}

			FMeshPolySplitShapePair NewPair; // grow a pair!
			NewPair.Mesh = Cast<UNavigationMeshBase>(StaticConstructObject(UNavigationMeshBase::StaticClass(),GetPylon()));
			NewPair.ShapeID = AffectingShapeIdx;

			// remove each poly from the outer mesh, and add it to new temp mesh
			FNavMeshPolyBase* CurPoly = NULL;
			for(INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
			{
				CurPoly = Polys(PolyIdx);

				// there is some slight snapping that goes on when creating sub-meshes so it's possible adjacent polys are barely colliding
				// with border shape that should not be.. test against poly center to avoid false positives
				if(FNavMeshPolyBase::ContainsPoint(CurShapeInfo.Shape,CurPoly->GetPolyCenter(WORLD_SPACE)))
				{
					NewPair.Mesh->CopyPolyIntoMesh(Polys(PolyIdx));
					RemovePoly(Polys(PolyIdx));
				}
			}

			// simplify cute new little mesh.  aww it's so cute
			NewPair.Mesh->MergePolys(FVector(1.f),FALSE);

			out_InternalMeshes.AddItem(NewPair);
		}
	}

	// lastly, simplify base mesh MT->this should be fast since the mesh is already simplified 
	MergePolys(FVector(1.f),FALSE);
	

	
	return TRUE;
}

/**
* BuildSubMeshForPoly
*  Will build a sub-mesh for the given polygon based on the obstacles registered that affect
*  that poly
* @param PolyIdx - the index of the poly being built
* @return - TRUE if successful
*/
UBOOL bMergeDynamicMeshes=TRUE;
UBOOL bCreateSubMeshEdges=TRUE;
UBOOL UNavigationMeshBase::BuildSubMeshForPoly(WORD PolyIdx, TArray<FPolyObstacleInfo*>& out_ObstaclesThatWereJustBuilt)
{
	//SCOPE_QUICK_TIMERX(BuildSubmesh,FALSE)

	
	// make sure edge delete notifications aren't sent out while wer'e cleaning up!
	SCOPED_EDGE_DELETE_HOLD(UnregisterObstacleWithNavMesh)

	FPolyObstacleInfo* Info = PolyObstacleInfoMap.Find(PolyIdx);

	if(Info == NULL)
	{
		// if we have no obstacles affecting us, bail!
		return FALSE;
	}
	else if(Info->bNeedRecompute == FALSE)
	{
		return FALSE;
	}


	out_ObstaclesThatWereJustBuilt.AddItem(Info);

	FNavMeshPolyBase* CurPoly = GetPolyFromId(PolyIdx);

	checkSlowish(CurPoly == Info->Poly);

	
	// reset whether we affect the obstacle mesh or not (because we don't know yet)
	Info->bAffectsDynamicObstacleMesh=FALSE;

	// if we have an existing mesh, flush dynamic edges on it, clean things up
	if(Info->SubMesh != NULL)
	{
		// if this submesh has polys on the dynamic obstacle mesh we need to rebuild it after this guy is built, mark it as such
		if( Info->ObstacleMeshPolys.Num() > 0 )
		{
			Info->bAffectsDynamicObstacleMesh = TRUE;
		}
		Info->SubMesh->CleanupMeshReferences(Info);
		SubMeshToParentPolyMap.Remove(Info->SubMesh);
		Info->SubMesh=NULL;
	}

	// if we have no obstacles, then just stop here.
	if(CurPoly->NumObstaclesAffectingThisPoly < 1)
	{
		return FALSE;
	}


	UNavigationMeshBase * NewMesh = Cast<UNavigationMeshBase>(StaticConstructObject(UNavigationMeshBase::StaticClass(),GetPylon()));

	NewMesh->InitTransform(GetPylon());

	// if we already had a mesh, it should now get GCd
	Info->SubMesh = NewMesh;
	// save mapping back to this polyID(parent polyID)
	SubMeshToParentPolyMap.Set(NewMesh,PolyIdx);

	// if there are no obstacles affecting this poly, then just leave a blank mesh!
	if(CurPoly->NumObstaclesAffectingThisPoly == 0)
	{
		return FALSE;
	}

	
	// 1. create submesh, add this poly as one big poly to submesh
	static TArray<FVector> CurPolyVerts;
	CurPolyVerts.Reset();

	for(INT VertIdx=0;VertIdx<CurPoly->PolyVerts.Num();++VertIdx)
	{
		CurPolyVerts.AddItem(CurPoly->GetVertLocation(VertIdx,WORLD_SPACE));
	}
	NewMesh->AddPoly(CurPolyVerts,CurPoly->PolyHeight,WORLD_SPACE);

	
	// build a list of shape infos to split the mesh with 
	static TArray<FMeshSplitingShape> Shapes;
	Shapes.Reset();


	// keep track of the obstacles we're IDing with shapes so we can easily get back to the pointer later for edge fixup
	static TArray<IInterface_NavMeshPathObstacle*> Obstacles;
	Obstacles.Reset();

	// build a shape descriptor for each obstacle to pass to SplitMeshAboutShape
	for(INT AffectingObstacleIdx=0;AffectingObstacleIdx<Info->LinkedObstacles.Num();++AffectingObstacleIdx)
	{
		IInterface_NavMeshPathObstacle* CurObstacle = Info->LinkedObstacles(AffectingObstacleIdx);
		
		for(INT ShapeIdx=0;ShapeIdx<CurObstacle->GetNumBoundingShapes();++ShapeIdx)
		{
			FMeshSplitingShape ShapeInfo;	
			
			if(!CurObstacle->GetBoundingShape(ShapeInfo.Shape,ShapeIdx))
			{
				continue;
			}

			ShapeInfo.bKeepInternalGeo = CurObstacle->PreserveInternalPolys();
			ShapeInfo.Height = -1.f;
			ShapeInfo.ID = Shapes.Num();
			Shapes.AddItem(ShapeInfo);
			Obstacles.AddItem(CurObstacle);
		}
	}

	
	static TArray<FMeshPolySplitShapePair> InternalMeshes;
	InternalMeshes.Reset();

	if(! NewMesh->SplitMeshAboutShapes(Shapes,InternalMeshes))
	{
		return FALSE;
	}

	// construct a temp map keyed on ID because FixupForSaving will change addresses of everything
	static TMap<INT, IInterface_NavMeshPathObstacle*> TempMap;
	TempMap.Reset();

	// copy polys from internal meshes into the real mesh
	for(INT InternalMeshIdx=0;InternalMeshIdx<InternalMeshes.Num();++InternalMeshIdx)
	{
		FMeshPolySplitShapePair& Pair = InternalMeshes(InternalMeshIdx);
		for(PolyList::TIterator It(Pair.Mesh->BuildPolys.GetTail());It;)
		{	
			FNavMeshPolyBase* CurPoly = *It;
			--It;
			FNavMeshPolyBase* NewInternalPoly = NewMesh->CopyPolyIntoMesh(CurPoly);
			if(NewInternalPoly != NULL)
			{
				TempMap.Set(NewMesh->BuildPolys.Num()-1,Obstacles(Pair.ShapeID));
			}
		}
	}

	
	// 4. finalize submesh
	NewMesh->FixupForSaving();

	// 5. initialize KDOP for submesh
	NewMesh->BuildKDOP();


	// now build map keyed on poly ptr for quick lookups of PolyID->Obstacle
	for(TMap<INT, IInterface_NavMeshPathObstacle*>::TIterator It(TempMap);It;++It)
	{
		FNavMeshPolyBase* CurPoly = NewMesh->GetPolyFromId(It.Key());
		NewMesh->SubMeshPolyIDToLinkeObstacleMap.Set(CurPoly,It.Value());
	}

	// cleanup temp meshes
	for(INT InternalMeshIdx=0;InternalMeshIdx<InternalMeshes.Num();++InternalMeshIdx)
	{
		FMeshPolySplitShapePair& Pair = InternalMeshes(InternalMeshIdx);
		for(PolyList::TIterator It(Pair.Mesh->BuildPolys.GetTail());It;)
		{	
			FNavMeshPolyBase* CurPoly = *It;
			--It;
			delete CurPoly;
		}
		Pair.Mesh->BuildPolys.Clear();
		Pair.Mesh = NULL;
	}

	return TRUE;
}

/**
 * Called after a submesh is built for a poly, allows metadata to get relinked to the new polys
 * @param PolyID - index of poly whose submesh was just built
 * @param SubMesh - submesh just built for this poly
 */
void UNavigationMeshBase::RebuildMetaDataLinkageForSubMesh(WORD PolyID, UNavigationMeshBase* SubMesh)
{
	FNavMeshPolyBase* CurPoly = GetPolyFromId(PolyID);

	// so far the only metadata we have is cover :D 
	for(INT CoverIdx=0;CoverIdx < CurPoly->PolyCover.Num(); ++CoverIdx)
	{
		FCoverReference& CovRef = CurPoly->PolyCover(CoverIdx);
		ACoverLink* Link = Cast<ACoverLink>(*CovRef);
		if( Link != NULL )
		{
			const FVector SlotLoc = Link->GetSlotLocation(CovRef.SlotIdx);
			FNavMeshPolyBase* SubMeshPolyThatContainsCover = SubMesh->GetPolyFromPoint(SlotLoc, NAVMESHGEN_MIN_WALKABLE_Z,WORLD_SPACE);
			if( SubMeshPolyThatContainsCover != NULL )
			{
				SubMeshPolyThatContainsCover->AddCoverReference(CovRef);
			}
		}
	}
}


/**
* This will ensure that the state of dynamic obstacles is up to date, and ready for 
* use by an entity
*/
UBOOL bCreateDynamicObstacleMesh=TRUE;
void UNavigationMeshBase::UpdateDynamicObstacles(TArray<FPolyObstacleInfo*>& ObstaclesThatWereJustBuilt)
{
	// make sure edge delete notifications aren't sent out while we're cleaning up!
	SCOPED_EDGE_DELETE_HOLD(UnregisterObstacleWithNavMesh)

	if(bNeedsObstacleRecompute)
	{
		//SCOPE_QUICK_TIMERX(UpdateDynamicObstacles,FALSE)
		{
			//SCOPE_QUICK_TIMERX(BuildSubmeshes,FALSE)

			for( PolyObstacleInfoList::TIterator It(PolyObstacleInfoMap);It;++It)
			{
				FPolyObstacleInfo& Info = It.Value();
				if(Info.bNeedRecompute)
				{
					BuildSubMeshForPoly(Info.Poly->Item, ObstaclesThatWereJustBuilt);
				}
			}
		}
	}
}

/**
 * builds all submeshes that need building (only geo, no edges/obstacle mesh)
 * @param out_ObstaclesThatWereJustBuild - list of obstacle infos for the submeshes we just built
 */
void UNavigationMeshBase::BuildAllSubMeshGeometry(TArray<FPolyObstacleInfo*>& out_ObstaclesThatWereJustBuilt)
{
	// make sure edge delete notifications aren't sent out while we're cleaning up!
	SCOPED_EDGE_DELETE_HOLD(BuildAllSubMeshGeometry)

	if(bNeedsObstacleRecompute)
	{
		{
			//SCOPE_QUICK_TIMERX(BuildSubmeshes,FALSE)

			for( PolyObstacleInfoList::TIterator It(PolyObstacleInfoMap);It;++It)
			{
				FPolyObstacleInfo& Info = It.Value();
				if(Info.bNeedRecompute)
				{
					BuildSubMeshForPoly(Info.Poly->Item, out_ObstaclesThatWereJustBuilt);
				}
			}
		}
	}

}

/**
 * FinishSubMeshBuilds
 * - will run through all the submeshes in this navmesh that were just built and finish them up (build edges, built up obstacle mesh)
 * @param ObstaclesThatWereJustBuild - list of infos that we need to touch up
 */
void UNavigationMeshBase::FinishSubMeshBuilds(TArray<FPolyObstacleInfo*>& ObstaclesThatWereJustBuilt)
{

	if (ObstaclesThatWereJustBuilt.Num() == 0)
	{
		SetNeedsRecompute(FALSE);
		return;
	}

	// now we're up to date, build the obstacle mesh for all our dynamic polys
	//SCOPE_QUICK_TIMERX(BuildSubmeshObstacleMesh,FALSE)

	UNavigationMeshBase* ObsMesh = GetPylon()->DynamicObstacleMesh;
	
	if ( ObsMesh == NULL )
	{				
		ObsMesh = Cast<UNavigationMeshBase>(StaticConstructObject(UNavigationMeshBase::StaticClass(),GetPylon()));
		ObsMesh->InitTransform(GetPylon());
		GetPylon()->DynamicObstacleMesh = ObsMesh;
	}	
	

	// add new polys for submeshes that were just built
	UBOOL bNeedsKDOPRebuild=FALSE;
	{
		//SCOPE_QUICK_TIMERX(BuildObstacleMeshForSubMesh_PASS1,FALSE)
	
		for(INT ObstIdx = 0; ObstIdx< ObstaclesThatWereJustBuilt.Num(); ++ObstIdx)
		{
			FPolyObstacleInfo* Info = ObstaclesThatWereJustBuilt(ObstIdx);
			if( Info->Poly->NavMesh == this)
			{
				// if we actually added any geometry to the obstacle mesh, then mark it as we will need to rebuild the KDOP
				if ( BuildObstacleMeshForSubMesh(Info,ObsMesh,FALSE,TRUE,&Info->ObstacleMeshPolys) || Info->bAffectsDynamicObstacleMesh )
				{
					Info->bAffectsDynamicObstacleMesh=TRUE;
					bNeedsKDOPRebuild=TRUE;
				}
			}
		}
	}
	// possibly rebuild kdop if it needs it
	if( ( bNeedsKDOPRebuild ) || (!ObsMesh->KDOPInitialized) )
	{
		//SCOPE_QUICK_TIMERX(KDOP_PASS1,FALSE)

		ObsMesh->KDOPInitialized = FALSE;
		ObsMesh->BuildKDOP(TRUE);
		bNeedsKDOPRebuild=FALSE;
	}

	// build submesh edges
	{
		//SCOPE_QUICK_TIMERX(SubMeshEdgesLISTBUILD,FALSE)

		for(INT ObstIdx = 0; ObstIdx< ObstaclesThatWereJustBuilt.Num(); ++ObstIdx)
		{
			FPolyObstacleInfo* Info = ObstaclesThatWereJustBuilt(ObstIdx);

			if( Info->Poly->NavMesh == this)
			{
				BuildSubMeshEdgesForPoly(Info->Poly->Item, ObstaclesThatWereJustBuilt);
				Info->bNeedRecompute = FALSE;
			}
		}
	}

	{

		//SCOPE_QUICK_TIMERX(BuildObstacleMeshForSubMesh_PASS2,FALSE)

		// second pass for obstacle mesh (based on special edges that get added)
		for(INT ObstIdx = 0; ObstIdx< ObstaclesThatWereJustBuilt.Num(); ++ObstIdx)
		{
			FPolyObstacleInfo* Info = ObstaclesThatWereJustBuilt(ObstIdx);

			if( Info->Poly->NavMesh == this)
			{
				if ( BuildObstacleMeshForSubMesh(Info,ObsMesh,TRUE,TRUE,&Info->ObstacleMeshPolys) )
				{
					bNeedsKDOPRebuild=TRUE;
				}
			}
		}
	}


	// possibly rebuild kdop if it needs it
	if( bNeedsKDOPRebuild )
	{
		//SCOPE_QUICK_TIMERX(KDOP_PASS2,FALSE)

		ObsMesh->KDOPInitialized = FALSE;
		ObsMesh->BuildKDOP(TRUE);
		bNeedsKDOPRebuild=FALSE;
	}


	SetNeedsRecompute(FALSE);
}

/**
* GetPolyObstacleInfo will call into the map and try to find a FPolyObstacleInfo corresponding to the passed
* Poly
* @param Poly - the poly to lookup
* @return - the info found for the passed poly (NULL if none found)
*/
FPolyObstacleInfo* UNavigationMeshBase::GetPolyObstacleInfo(FNavMeshPolyBase* Poly)
{
	return PolyObstacleInfoMap.Find(Poly->Item);
}

/**
* Helper function which will return obstacle info for this poly (if any)
* @return - the found obstacle info (NULL if none found)
*/
FPolyObstacleInfo* FNavMeshPolyBase::GetObstacleInfo()
{
	return NavMesh->GetPolyObstacleInfo(this);
}
/*
 * GetSubMesh
 * - returns the submesh for this poly if one exists
 * @return - the submesh for this poly (NULL of none)
 */
UNavigationMeshBase* FNavMeshPolyBase::GetSubMesh()
{
	FPolyObstacleInfo* Info = GetObstacleInfo();
	if( Info == NULL )
	{
		return NULL;
	}
	return Info->SubMesh;
}

/**
 * @returns TRUE if this poly is within a submesh
 */
UBOOL FNavMeshPolyBase::IsSubMeshPoly()

{
	return NavMesh->IsSubMesh();
}

/**
 * returns the parent poly for this poly (NULL if this poly is not in a submesh)
 */
FNavMeshPolyBase* FNavMeshPolyBase::GetParentPoly()
{
	if (!IsSubMeshPoly())
	{
		return NULL;
	}

	// determine parent poly ID	
	UNavigationMeshBase* ParentMesh = NavMesh->GetTopLevelMesh();
	WORD* ParentID = ParentMesh->SubMeshToParentPolyMap.Find(NavMesh);
	checkSlowish(ParentID != NULL);
	return &ParentMesh->Polys(*ParentID);
}

/**
 * replaces IsA(NavigationPoint) check for primitivecomponents 
 */
UBOOL APathTargetPoint::ShouldBeHiddenBySHOW_NavigationNodes()
{
	return TRUE;
}

/**
 * ImportBuildPolys
 *  imports BuildPolys list from external source, calculating all derived values and skipping over invalid items.
 * @param InVerts - list of vert locations
 * @param InPolys - list of polygons to import (must have: PolyVerts, PolyHeight)
 * @param InBorderPolys - border information, matching size of InPolys array (1 = poly on navmesh boundary)
 * @param BoundsZOffset - height offset for lowering poly bounds
 */
void UNavigationMeshBase::ImportBuildPolys(TArray<FMeshVertex>& InVerts, TArray<FNavMeshPolyBase*>& InPolys, const TArray<BYTE>& InBorderPolys, FLOAT BoundsZOffset)
{
	// fill vertex list and hash
	if (VertHash == NULL)
	{
		VertHash = new FVertHash();
	}

	Verts.Empty();
	for (INT iVert = 0; iVert < InVerts.Num(); iVert++)
	{
		Verts.AddItem(InVerts(iVert));
		VertHash->Add(Verts(iVert), iVert);
	}

	// fill build polys list
	BuildPolys.Clear();
	for (INT iPoly = InPolys.Num() - 1; iPoly >= 0; iPoly--)
	{
		FNavMeshPolyBase* PtrPolyItem = InPolys(iPoly);
		PtrPolyItem->NavMesh = this;

		// skip polys with invalid normal (e.g. snapped vertices)
		PtrPolyItem->PolyNormal = PtrPolyItem->CalcNormal();
		if (PtrPolyItem->PolyNormal.IsNearlyZero())
		{
			delete PtrPolyItem;
			continue;
		}

		// add to lists
		PtrPolyItem->Item = BuildPolys.Num();
		BuildPolys.AddTail(PtrPolyItem);

		if (InBorderPolys(iPoly) != 0)
		{
			BorderPolys.AddHead(PtrPolyItem);
			PtrPolyItem->BorderListNode = BorderPolys.GetHead();
		}

		// calculate bounds 
		FVector Up = GetPylon()->Up(PtrPolyItem);
		FVector PolyCenter = FVector(0.f);
		for (INT iVert = 0; iVert < PtrPolyItem->PolyVerts.Num(); iVert++)
		{
			FMeshVertex& VertItem = Verts(PtrPolyItem->PolyVerts(iVert));
			VertItem.ContainingPolys.AddItem(PtrPolyItem);

			PtrPolyItem->BoxBounds += VertItem + Up * PtrPolyItem->PolyHeight;
			PtrPolyItem->BoxBounds += VertItem - Up * BoundsZOffset;

			PolyCenter += VertItem;
		}

		PolyCenter /= PtrPolyItem->PolyVerts.Num();
		PtrPolyItem->SetPolyCenter(PolyCenter);

		// add to octree
		if(PtrPolyItem->OctreeId.IsValidId())
		{
			RemovePolyFromOctree(PtrPolyItem);
		}
		AddPolyToOctree(PtrPolyItem);
	}
}

/**
 * RuntimeMoveVertex
 *  moves a vertex of already built poly and updates all runtime structures of navmesh and obstacle mesh
 *  apart from bounds and KDOP tree
 * @param VertId - Id of vertex to move
 * @param Location - new vertex location in world space
 * @param BoundsZOffset - height offset for lowering poly bounds
 */
void UNavigationMeshBase::RuntimeMoveVertex(VERTID VertId, const FVector& Location, FLOAT BoundsZOffset)
{
	UNavigationMeshBase* ObsMesh = GetObstacleMesh();
	if (ObsMesh == NULL || ObsMesh == this) 
		return;

	//Update the vert location
	FMeshVertex& VertInfo = Verts(VertId);
	FVector OrgLocation = L2WTransformFVector(VertInfo);
	VertInfo = W2LTransformFVector(Location);

	TArray<FLOAT> PolyHeights;
	PolyHeights.AddItem(0.0f);

	//Fix up poly bounds
	for (INT iPoly = 0; iPoly < VertInfo.PolyIndices.Num(); iPoly++)
	{
		FNavMeshPolyBase& Poly = Polys(VertInfo.PolyIndices(iPoly));
		FVector PolyUp(0,0,1);

		Poly.BoxBounds = FBox(0);
		Poly.RecalcAfterVertChange(&PolyUp);

		for (INT iVert = 0; iVert < Poly.PolyVerts.Num(); iVert++)
		{
			FVector VertLoc = Poly.GetVertLocation(iVert, LOCAL_SPACE);
			Poly.BoxBounds += VertLoc + PolyUp * Poly.PolyHeight;
			Poly.BoxBounds += VertLoc - PolyUp * BoundsZOffset;
		}

		PolyHeights.AddUniqueItem(Poly.PolyHeight);
	}

	//Update the verts for the obstacle mesh 
	TArray<INT> ObstacleMeshPolys;
	for (INT iOffset = 0; iOffset < PolyHeights.Num(); iOffset++)
	{
		FVector LocalVertLoc = ObsMesh->W2LTransformFVector(OrgLocation + FVector(0,0,PolyHeights(iOffset)));
		for (INT iVert = 0; iVert < ObsMesh->Verts.Num(); iVert++)
		{
			if (LocalVertLoc.Equals(ObsMesh->Verts(iVert), 5.0f))
			{
				FMeshVertex& ObsVertInfo = ObsMesh->Verts(iVert);
				ObsVertInfo = ObsMesh->W2LTransformFVector(Location + FVector(0,0,PolyHeights(iOffset)));
				
				//Gather list of polys that need bounds updated
				for (INT iPoly = 0; iPoly < ObsVertInfo.PolyIndices.Num(); iPoly++)
				{
					ObstacleMeshPolys.AddUniqueItem(ObsVertInfo.PolyIndices(iPoly));
				}

				break;
			}
		}
	}

	//Update obstacle mesh poly bounds
	for (INT iPoly = 0; iPoly < ObstacleMeshPolys.Num(); iPoly++)
	{
		FNavMeshPolyBase& Poly = ObsMesh->Polys(ObstacleMeshPolys(iPoly));
		FVector PolyUp(0,0,1);

		Poly.BoxBounds = FBox(0);
		Poly.RecalcAfterVertChange(&PolyUp);
	}
	
	// TODO: edges?
}

/**
 * sets up VertHash mapping, required for FindVert in runtime
 */
void UNavigationMeshBase::PrepareVertexHash()
{
	if (VertHash == NULL)
	{
		VertHash = new FVertHash();
		VertHash->Empty(Verts.Num());

		// recreate hash and add to hashmap
		for (INT VertIdx = 0; VertIdx < Verts.Num(); VertIdx++)
		{
			Verts(VertIdx) = FMeshVertex(Verts(VertIdx));
			VertHash->Add(Verts(VertIdx), VertIdx);
		}
	}
}

/** 
 * Applies dynamic snap data to mesh vertices and recalculates bounds and properties of affected polys
 * @param SnapData - vertex move data
 * @param BoundsZOffset - height offset for lowering poly bounds
 */
void UNavigationMeshBase::ApplyDynamicSnap(const TArray<FDynamicSnapInfo>& SnapData, FLOAT BoundsZOffset)
{
	TArray<FDynamicSnapInfo> RevertData;
	for (INT i = 0; i < SnapData.Num(); i++)
	{
		const FDynamicSnapInfo& SnapInfo = SnapData(i);
		FMeshVertex& VertInfo = Verts(SnapInfo.VertId);

		FDynamicSnapInfo NewRevertInfo;
		NewRevertInfo.VertId = SnapInfo.VertId;
		NewRevertInfo.VertWorldLocation = L2WTransformFVector(VertInfo);
		RevertData.AddItem(NewRevertInfo);

		RuntimeMoveVertex(SnapInfo.VertId, SnapInfo.VertWorldLocation, BoundsZOffset);
	}

	if (SnapData.Num() > 0)
	{
		BuildBounds();

		UNavigationMeshBase* ObsMesh = GetObstacleMesh();
		if (ObsMesh != NULL && ObsMesh != this)
		{
			ObsMesh->BuildBounds();
			ObsMesh->ForcedBuildKDOP(FALSE);
		}
	}

	DynamicSnapRevertData = RevertData;
}

/**
 * Reverts all changes caused by dynamic snap
 */
void UNavigationMeshBase::RevertDynamicSnap()
{
	if (!bSkipDynamicSnapRevert)
	{
		ApplyDynamicSnap(DynamicSnapRevertData, RecastBoundsZOffset);
		DynamicSnapRevertData.Empty();
	}
}
