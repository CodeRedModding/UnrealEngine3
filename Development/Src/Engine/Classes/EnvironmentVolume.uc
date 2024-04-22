
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Used to define certain gameplay areas, and has optional support for interacting with the NavMesh
 * (To block some types of enemies from pathing through it).
 */

class EnvironmentVolume extends Volume
	native(AI)
	implements(Interface_NavMeshPathObstacle)
	implements(Interface_NavMeshPathObject)
	placeable;

/** Keep track is NavMesh is split by this volume to affect AI pathing. 
 *  Change this through SetSplitNavMesh() */
var const transient bool  bSplitNavMesh;

cpptext
{
	virtual UBOOL ShouldAIAvoidMe(AAIController* AIC);

	virtual void BeginDestroy()
	{
		Super::BeginDestroy();

		// Unsplit if it was split.
		if( bSplitNavMesh )
		{
			SetSplitNavMesh(FALSE);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Interface_NavMeshPathObject
	/** 
	 *  Called from edges linked to this PO
	 *  @param Interface     - the navhandle interface of the entity pathing 
	 *  @param PreviousPoint - the previous point in the path search (e.g. the point along the predecessor edge)
	 *  @param out_PathEdgePoint - the point we used along this edge to determine the cost 
	 *  @param Edge - the edge linked to this PO which needs to compute cost	 
	 *  @return     - the cost for traversing this edge
	 */
	virtual INT CostFor( const FNavMeshPathParams& PathParams, const FVector& PreviousPoint, FVector& out_PathEdgePoint, FNavMeshPathObjectEdge* Edge, FNavMeshPolyBase* SourcePoly );
	
	/**
	 * called to allow this PO to draw custom stuff for edges linked to it
	 * @param DRSP          - the sceneproxy we're drawing for
	 * @param DrawOffset    - offset from the actual location we should be drawing 
	 * @param Edge          - the edge we're drawing
	 * @return - whether this PO is doing custom drawing for the passed edge (FALSE indicates the default edge drawing functionality should be used)
	 */
	virtual UBOOL DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset, FNavMeshPathObjectEdge* Edge );
	// END path object interface
	//////////////////////////////////////////////////////////////////////////


	//////////////////////////////////////////////////////////////////////////
	// Interface_NavMeshPathObstacle 
	/**
	 * this function should populate out_polyshape with a list of verts which describe this object's 
	 * convex bounding shape
	 * @param out_PolyShape - output array which holds the vertex buffer for this obstacle's bounding polyshape
	 * @return TRUE if this object should block things right now (FALSE means this obstacle shouldn't affect the mesh)
	 */
	virtual UBOOL GetBoundingShape(TArray<FVector>& out_PolyShape,INT ShapeIdx);

	/**
	 * when TRUE polys internal to this obstacle will be preserved, but still split. (useful for things like cost volumes that 
	 * need to adjust cost but not completely destroy parts of the mesh
	 * @return TRUE if polys should be preserved internal to this obstacle
	 */
	virtual UBOOL PreserveInternalPolys() { return TRUE; }

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
	 * @(optional) param EdgeGroupID - ID of the edgegroup this edge is a part of (defaults to no group)
	 * @return returns an enum describing what just happened (what actions did we take) - used to determien what accompanying actions need to be taken 
	 *         by other obstacles and calling code
	 */
	virtual EEdgeHandlingStatus AddObstacleEdge( EEdgeHandlingStatus Status, const FVector& inV1, const FVector& inV2, TArray<FNavMeshPolyBase*>& ConnectedPolys, UBOOL bEdgesNeedToBeDynamic, INT PolyAssocatedWithThisPO, FLOAT SupportedEdgeWidth=-1.0f, BYTE EdgeGroupID=MAXBYTE);


	/**
	 * For debugging.  Verifies that this pathobject is still alive and well and not orphaned or deleted
	 * @return - TRUE If this path object is in good working order
	 */
	virtual UBOOL Verify()
	{
		return !IsPendingKill();
	}

	virtual UBOOL VerifyObstacle()
	{
		return Verify();
	}

	// END Interface_NavMeshPathObstacle
	//////////////////////////////////////////////////////////////////////////

	/**
	 * Removes the volume from world info's list of environment volumes.
	 */
	virtual void ClearComponents( void );

protected:
	/**
	 * Adds the volume to world info's list of environment volumes.
	 */
	virtual void UpdateComponentsInternal( UBOOL bCollisionUpdate = FALSE );
public:
}

/** Should this volume register and split the NavMesh to affect AI pathing? */
final native function SetSplitNavMesh(bool bNewValue);

defaultproperties
{
	bSplitNavMesh=FALSE // That should not be changed, only through SetSplitNavMesh().
}
